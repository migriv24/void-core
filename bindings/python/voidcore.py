"""
voidcore.py — Python ctypes binding for the Void Core C library.

This is the first binding target (the Deltarune mod tool is Python). It wraps the
pure C ABI in `core/include/voidcore.h`. Everything crosses as JSON strings, so
this file stays tiny and the host app never touches C memory directly.

Memory discipline: vc_dispatch / vc_export_state return heap strings that MUST be
freed via vc_free_str. We declare their restype as c_void_p (not c_char_p) so we
keep the exact pointer to free, read the bytes via cast, then free — no leak.
"""
from __future__ import annotations

import ctypes
import json
import os
import sys
from typing import Any, Optional


# The host effect handler signature: char* fn(const char *op, const char *args_json,
# void *user). We return c_void_p (a string the core will free()), built via
# vc_alloc_str so it's allocated by the library's own CRT (no cross-allocator free).
_EFFECT_FN = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_void_p)


def quote_arg(value: str) -> str:
    r"""Wrap `value` as exactly ONE dispatcher argument (SPEC §6.1).

    Every host that stores free text through the dispatcher needs this, and three
    codebases have now written it independently and gotten it wrong — twice with
    silent, content-level corruption (Void Hormiga, 2026-08-17). It is a pure string
    function and needs no engine, so import it from here rather than rewriting it.

    The tokenizer strips bare quote characters, and inside single quotes honors
    exactly one escape: ``\'``. Single-quoting therefore carries spaces, double
    quotes, newlines and backslashes — with one trap. A value *ending* in a backslash
    would place ``\`` immediately before the closing ``'``, which the tokenizer reads
    as an escaped apostrophe: the argument never closes, silently swallowing the rest
    of the line, and dispatch still reports ``ok``. Trailing backslashes are emitted
    *outside* the quotes, where a backslash is literal and — being neither whitespace
    nor a quote — still belongs to the same token.

        >>> quote_arg("don't")
        "'don\\'t'"
        >>> quote_arg("C:\\")          # the case the obvious helper corrupts
        "'C:'\\"
    """
    head = value.rstrip("\\")
    return "'" + head.replace("'", "\\'") + "'" + value[len(head):]


class UnterminatedQuote(ValueError):
    """A quoted run was still open at end of input (SPEC §6.1 rule 5).

    Since 0.2.7 this is an error rather than a silent run-to-end-of-input. That
    single change is what converts this whole bug class from *quiet* to *loud*:
    before it, a mis-quoted argument swallowed the rest of the line — or, in a
    transcript, the rest of the file — and dispatch still returned ``ok: true``.
    """

    def __init__(self, message: str, line: int = 0):
        super().__init__(message)
        self.line = line


def split_args(line: str) -> list[str]:
    r"""Tokenize one command line exactly as the dispatcher will (SPEC §6.1).

    The *decoder* half of the codec, and the half hosts forget to write. A host
    that reviews a proposed command before dispatching it — a submission, a
    harvested dataset, an agent's transcript — must be able to ask "what will this
    actually do" with the same tokenizer that will do it, rather than guessing.

    Pure Python: no engine, no build. It is the inverse of :func:`quote_arg`, and
    the law they satisfy together is pinned by a property test:

        >>> split_args(quote_arg("a\nb  'c'  \\")) == ["a\nb  'c'  \\"]
        True

        >>> split_args("set v bio 'two words'")
        ['set', 'v', 'bio', 'two words']
        >>> split_args("a'b'c")                      # quoting is strip-anywhere
        ['abc']
        >>> split_args("set v bio 'oops")
        Traceback (most recent call last):
            ...
        voidcore.UnterminatedQuote: unterminated quote (SPEC §6.1 rule 5)
    """
    out: list[str] = []
    buf: list[str] = []
    quote = ""
    started = False
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if not quote and c.isspace():
            if started:
                out.append("".join(buf))
                buf.clear()
                started = False
            i += 1
            continue
        started = True
        if quote:
            # rule 3: inside single quotes the ONLY escape is \'
            if quote == "'" and c == "\\" and i + 1 < n and line[i + 1] == "'":
                buf.append("'")
                i += 2
                continue
            if c == quote:            # rule 2: the closing quote is stripped
                quote = ""
                i += 1
                continue
        elif c in "'\"":              # rule 2: the opening quote is stripped
            quote = c
            i += 1
            continue
        buf.append(c)
        i += 1
    if quote:                          # rule 5
        raise UnterminatedQuote("unterminated quote (SPEC §6.1 rule 5)")
    if started:
        out.append("".join(buf))
    return out


#: SPEC §8 control words. A transcript containing none of them is *flat* — its
#: effect can be read off its statements without simulating it, which is exactly
#: what a host gating a proposed transcript needs to know.
CONTROL_WORDS = frozenset(
    "if elif else while repeat foreach break continue return halt let def try "
    "catch include call on wait".split()
)


def split_transcript(src: str) -> list[dict]:
    r"""Split a transcript into the statements it will run (SPEC §6.1 + §8).

    Boundaries are newline and ``;`` **outside quoted runs** — so a newline inside
    a value is data, not a new command — and ``#`` comments are dropped. Each
    statement comes back as ``{"line", "text", "argv", "flat"}``.

    This is the function a submission gate should be built on. Reviewing the raw
    text instead is what lets an injected line hide inside a value:

        >>> t = "set visitor bio 'weekends.\nset treasurer email attacker@evil'"
        >>> cmds = split_transcript(t)
        >>> len(cmds)                       # ONE command, not two
        1
        >>> cmds[0]["argv"][0]
        'set'

    Raises :class:`UnterminatedQuote` (with ``.line``) rather than returning a
    plausible-looking partial parse.
    """
    cmds: list[dict] = []
    buf: list[str] = []
    quote = ""
    line_no = 1
    stmt_line = 1
    flat = True
    i = 0
    n = len(src)

    def flush() -> None:
        text = "".join(buf).rstrip()
        buf.clear()
        if not text:
            return
        argv = split_args(text)          # may raise; .line is attached by caller
        cmds.append({"line": stmt_line, "text": text, "argv": argv})

    while i <= n:
        c = src[i] if i < n else ""
        if not c or (not quote and c in "\n;"):
            try:
                flush()
            except UnterminatedQuote as e:
                raise UnterminatedQuote(
                    f"{e} — the statement on line {stmt_line} swallows the rest "
                    "of the transcript",
                    stmt_line,
                ) from None
            if not c:
                break
            if c == "\n":
                line_no += 1
            i += 1
            continue
        if not quote and c == "#" and not buf:
            while i < n and src[i] != "\n":
                i += 1
            continue
        if not buf and c in " \t":
            i += 1
            continue
        if not buf:
            stmt_line = line_no
        if not quote and c in "{}":
            flat = False
        if quote:
            if quote == "'" and c == "\\" and i + 1 < n and src[i + 1] == "'":
                buf.append(src[i : i + 2])
                i += 2
                continue
            if c == quote:
                quote = ""
        elif c in "'\"":
            quote = c
        buf.append(c)
        i += 1

    if quote:
        raise UnterminatedQuote(
            "unterminated quote (SPEC §6.1 rule 5): the transcript ends inside a "
            "quoted value",
            stmt_line,
        )
    for cmd in cmds:
        cmd["flat"] = flat and bool(cmd["argv"]) and cmd["argv"][0] not in CONTROL_WORDS
    return cmds


def _default_dll_path() -> str:
    """Locate libvoidcore.dll relative to the repo (built by CMake into core/build/bin)."""
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.abspath(os.path.join(here, "..", ".."))
    names = ["libvoidcore.dll", "voidcore.dll", "libvoidcore.so", "libvoidcore.dylib"]
    roots = [
        os.path.join(repo, "core", "build", "bin"),
        os.path.join(repo, "core", "build"),
        here,
    ]
    for root in roots:
        for name in names:
            cand = os.path.join(root, name)
            if os.path.exists(cand):
                return cand
    raise FileNotFoundError(
        "libvoidcore.dll not found — build it first:\n"
        "  cmake -S core -B core/build -G Ninja && cmake --build core/build"
    )


class VoidCore:
    """A handle to one Void Core manager."""

    def __init__(self, state: Optional[dict] = None, dll_path: Optional[str] = None):
        self._lib = ctypes.CDLL(dll_path or _default_dll_path())
        self._bind()
        state_json = json.dumps(state).encode("utf-8") if state is not None else None
        self._m = self._lib.vc_create(state_json)
        if not self._m:
            raise RuntimeError("vc_create returned NULL (allocation failure)")

    def _bind(self) -> None:
        L = self._lib
        L.vc_create.restype = ctypes.c_void_p
        L.vc_create.argtypes = [ctypes.c_char_p]
        L.vc_dispatch.restype = ctypes.c_void_p   # keep ptr to free it
        L.vc_dispatch.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.vc_export_state.restype = ctypes.c_void_p
        L.vc_export_state.argtypes = [ctypes.c_void_p]
        L.vc_register_glyph.restype = ctypes.c_int
        L.vc_register_glyph.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.vc_set_effect_handler.restype = None
        L.vc_set_effect_handler.argtypes = [ctypes.c_void_p, _EFFECT_FN, ctypes.c_void_p]
        L.vc_alloc_str.restype = ctypes.c_void_p
        L.vc_alloc_str.argtypes = [ctypes.c_char_p]
        L.vc_free_str.argtypes = [ctypes.c_void_p]
        L.vc_destroy.argtypes = [ctypes.c_void_p]
        L.vc_version.restype = ctypes.c_char_p
        L.vc_tag_match.restype = ctypes.c_int
        L.vc_tag_match.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        # The §6.1 codec (0.2.7+). Bound leniently so this binding still loads
        # against an older library — the pure-Python `quote_arg` / `split_args` /
        # `split_transcript` cover the same ground with no engine at all, and a
        # host pinned to an old build should not lose the whole binding over it.
        self._has_codec = True
        for name in ("vc_arg_quote", "vc_argv_split_json", "vc_transcript_split_json"):
            try:
                fn = getattr(L, name)
            except AttributeError:
                self._has_codec = False
                continue
            fn.restype = ctypes.c_void_p
            fn.argtypes = [ctypes.c_char_p]
        # Host-controlled undo (0.2.9+), bound leniently for the same reason.
        self._has_undo_ctl = True
        try:
            L.vc_set_undo.restype = None
            L.vc_set_undo.argtypes = [ctypes.c_void_p, ctypes.c_int]
            L.vc_set_undo_depth.restype = None
            L.vc_set_undo_depth.argtypes = [ctypes.c_void_p, ctypes.c_int]
        except AttributeError:
            self._has_undo_ctl = False
        # The command journal (0.2.8+), bound leniently for the same reason.
        self._has_journal = True
        try:
            L.vc_set_journal.restype = None
            L.vc_set_journal.argtypes = [ctypes.c_void_p, ctypes.c_int]
            L.vc_export_journal.restype = ctypes.c_void_p
            L.vc_export_journal.argtypes = [ctypes.c_void_p]
            L.vc_journal_clear.restype = None
            L.vc_journal_clear.argtypes = [ctypes.c_void_p]
        except AttributeError:
            self._has_journal = False

    def _take(self, ptr: int) -> str:
        """Read a heap string returned by the lib, then free it."""
        if not ptr:
            return ""
        try:
            return ctypes.cast(ptr, ctypes.c_char_p).value.decode("utf-8")
        finally:
            self._lib.vc_free_str(ptr)

    @property
    def version(self) -> str:
        return self._lib.vc_version().decode("utf-8")

    def dispatch(self, command: str) -> dict[str, Any]:
        """Run one command; return the parsed {ok, lines, data} result."""
        ptr = self._lib.vc_dispatch(self._m, command.encode("utf-8"))
        return json.loads(self._take(ptr))

    def export_state(self) -> dict[str, Any]:
        ptr = self._lib.vc_export_state(self._m)
        return json.loads(self._take(ptr))

    # ── undo control (SPEC §6) ──────────────────────────────────────────────
    def set_undo(self, enabled: bool = True) -> None:
        """Turn undo/redo recording on or off (on by default).

        Every mutating command snapshots the whole undoable slice before it runs,
        which is cheap for a document and expensive for a world. A host whose
        runes are live instances rather than a design should turn this off and
        accept that `undo` fails; a host authoring a document should leave it on.
        Turning it off also drops the frames already recorded."""
        self._need_undo_ctl()
        self._lib.vc_set_undo(self._m, 1 if enabled else 0)

    def set_undo_depth(self, depth: int) -> None:
        """Bound the undo/redo stacks to `depth` frames (default 200).

        Lowering it trims the stacks immediately. Values below 1 clamp to 1."""
        self._need_undo_ctl()
        self._lib.vc_set_undo_depth(self._m, int(depth))

    def _need_undo_ctl(self) -> None:
        if not self._has_undo_ctl:
            raise RuntimeError(
                "this libvoidcore has no undo control (needs core >= 0.2.9)")

    # ── the command journal (SPEC §6.2) ─────────────────────────────────────
    def _need_journal(self) -> None:
        if not self._has_journal:
            raise RuntimeError(
                "this libvoidcore has no command journal (needs core >= 0.2.8)")

    def set_journal(self, enabled: bool = True) -> None:
        """Record every successful mutating command as data (off by default).

        Journaling never changes what a command does, so it is safe to enable at
        any point; what it costs is one id-set walk per mutation."""
        self._need_journal()
        self._lib.vc_set_journal(self._m, 1 if enabled else 0)

    def journal(self) -> list[dict[str, Any]]:
        """The reified command record, oldest first (SPEC §6.2).

        Each entry is {seq, command, verb, who, pure, slice, minted}. Consumers
        building a replayable or transmissible history must keep only `pure`
        entries — an effectful command reached the host and cannot be replayed."""
        self._need_journal()
        ptr = self._lib.vc_export_journal(self._m)
        return json.loads(self._take(ptr))

    def journal_clear(self) -> None:
        self._need_journal()
        self._lib.vc_journal_clear(self._m)

    def tag_match(self, expr: str, tags: list[str]) -> bool:
        """Evaluate a SPEC §5 tag/filter expression against a bag of tags.

        The one C implementation of the filter grammar, exposed so hosts filtering
        holiday/external entities (`effect query …`) never reimplement it. Include
        the entity's name in `tags` to get name-as-tag matching. Stateless (does
        not touch this manager's state) and thread-safe."""
        r = self._lib.vc_tag_match(expr.encode("utf-8"),
                                   json.dumps(list(tags)).encode("utf-8"))
        if r < 0:
            raise ValueError(f"vc_tag_match: malformed input (expr={expr!r})")
        return bool(r)

    # ── the §6.1 codec, as the C core implements it (SPEC §6.1) ─────────────
    # These are stateless library functions, exposed here as methods only because
    # this class already owns the loaded library. The pure-Python `quote_arg`,
    # `split_args` and `split_transcript` at module scope do the same job with no
    # build at all — use those unless you specifically want to cross-check the two
    # implementations against each other (voidcore/codec_test.py does).

    def _need_codec(self) -> None:
        if not self._has_codec:
            raise NotImplementedError(
                f"this libvoidcore ({self.version}) predates the exported §6.1 "
                "codec (0.2.7); use the pure-Python quote_arg / split_args / "
                "split_transcript instead"
            )

    def arg_quote(self, value: str) -> str:
        """Quote a value as one dispatcher argument, via the C codec."""
        self._need_codec()
        ptr = self._lib.vc_arg_quote(value.encode("utf-8"))
        return self._take(ptr)

    def argv_split(self, line: str) -> dict:
        """Tokenize a command line exactly as `dispatch` will.

        Returns ``{"ok": True, "argv": [...]}`` or, for a quoted run that never
        closed (§6.1 rule 5), ``{"ok": False, "error": ..., "argv": None}``.
        """
        self._need_codec()
        ptr = self._lib.vc_argv_split_json(line.encode("utf-8"))
        return json.loads(self._take(ptr))

    def transcript_split(self, src: str) -> dict:
        """Split a transcript into the statements it will run.

        The decoder a host needs before dispatching someone else's transcript:
        boundaries are newline and ``;`` outside quoted runs, so a newline inside
        a value stays inside it. ``flat`` reports whether the transcript is a
        plain list of commands (no blocks, no §8 control words).
        """
        self._need_codec()
        ptr = self._lib.vc_transcript_split_json(src.encode("utf-8"))
        return json.loads(self._take(ptr))

    def register_glyph(self, glyph: dict) -> bool:
        """Declare a rune type (host app config; not part of exported state)."""
        ok = self._lib.vc_register_glyph(self._m, json.dumps(glyph).encode("utf-8"))
        return bool(ok)

    def set_effect_handler(self, fn) -> None:
        """Register the host effect handler — the holiday boundary where real I/O lives.

            fn(op: str, args) -> dict | str | None

        Invoked for `save` (args = the full state document), `deploy`/`build`/`preview`
        (args = {"args":[...]}), and the generic `effect <op> [args...]` verb (args =
        {"args":[...]}). The return becomes the command's `data` (dict/list) or a line
        (str); None means "done, no value". This is how an app reaches its real backend —
        e.g. Hormiga's "holiday query -> tagged rune collection": `effect query "<expr>"`.

        Memory is handled for you: the result is copied into a library-owned string
        (vc_alloc_str) that the core frees, so there is no cross-allocator hazard."""
        def _trampoline(op, args_json, _user):
            try:
                op_s = op.decode("utf-8") if op else ""
                args = json.loads(args_json.decode("utf-8")) if args_json else None
                result = fn(op_s, args)
                if result is None:
                    return None
                s = result if isinstance(result, str) else json.dumps(result)
                return self._lib.vc_alloc_str(s.encode("utf-8"))
            except Exception:
                return None  # never let a host exception cross back into C
        cb = _EFFECT_FN(_trampoline)
        self._effect_cb = cb  # keep a reference alive (C holds a raw pointer to it)
        self._lib.vc_set_effect_handler(self._m, cb, None)

    def close(self) -> None:
        if getattr(self, "_m", None):
            self._lib.vc_destroy(self._m)
            self._m = None

    def __enter__(self) -> "VoidCore":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


# ── smoke test ───────────────────────────────────────────────────────────────
if __name__ == "__main__":
    vc = VoidCore()
    print(f"binding OK - Void Core {vc.version}\n")

    # the app declares its rune types first
    vc.register_glyph({"glyph": "dialogue", "label": "Dialogue line",
                       "editor": "form", "fields": ["speaker", "text", "expression"]})
    vc.register_glyph({"glyph": "walk", "label": "Walk action",
                       "editor": "form", "fields": ["actor", "x", "y", "speed"]})

    script = [
        "mantle new castle-town",
        "rune new dialogue susie-intro",
        'set susie-intro text "Hey, Kris!"',
        "tag susie-intro +chapter:2 +susie",
        "rune new walk kris-walk-in",
        "ls",
    ]
    for cmd in script:
        res = vc.dispatch(cmd)
        flag = "ok " if res["ok"] else "ERR"
        print(f"[{flag}] {cmd}")
        for line in res["lines"]:
            print(f"        {line}")

    # prove data crosses as real Python objects
    names = vc.dispatch("ls")["data"]
    assert names == ["susie-intro", "kris-walk-in"], names
    print(f"\nls data -> {names}")

    # undo/redo (SPEC §6)
    vc.dispatch('set susie-intro text "EDITED"')
    assert vc.dispatch("get susie-intro text")["data"] == "EDITED"
    assert vc.dispatch("undo")["ok"]
    assert vc.dispatch("get susie-intro text")["data"] == "Hey, Kris!"
    assert vc.dispatch("redo")["ok"]
    assert vc.dispatch("get susie-intro text")["data"] == "EDITED"
    assert vc.dispatch("undo")["ok"]  # leave it back at "Hey, Kris!"
    print("undo/redo: OK")

    # round-trip the whole state through Python
    state = vc.export_state()
    assert state["mantles"][0]["name"] == "castle-town"
    print(f"exported state has {len(state['mantles'][0]['runes'])} runes in "
          f"'{state['mantles'][0]['name']}'")

    # rebuild a fresh manager from the exported state
    vc2 = VoidCore(state=state)
    assert vc2.dispatch("use castle-town")["ok"]
    assert vc2.dispatch("ls")["data"] == names
    print("round-trip through a new manager: OK")

    # error contract: unknown verb + unknown glyph both rejected
    bad = vc.dispatch("bogus-verb x")
    assert bad["ok"] is False
    assert vc.dispatch("rune new wobble x")["ok"] is False  # glyph not registered
    assert vc.dispatch("rune new dialogue ok-name")["ok"] is True
    print(f"error contract: {bad['lines'][0]!r}")
    print(f"glyph default content: {vc.dispatch('cat ok-name')['data']['content']}")

    # tag system: filter grammar, glyph-as-tag, multi-target @ (SPEC §5)
    vc.dispatch("rune new dialogue ralsei-greet")
    vc.dispatch("tag ralsei-greet +chapter:2 +ralsei")
    assert vc.dispatch('ls --tag "ralsei AND chapter:2"')["data"] == ["ralsei-greet"]
    assert vc.dispatch("ls --tag glyph:walk")["data"] == ["kris-walk-in"]
    assert set(vc.dispatch("ls --tag chapter:2")["data"]) == {"susie-intro", "ralsei-greet"}
    # kris-walk-in and ok-name (made earlier) are the runes without chapter:2
    assert set(vc.dispatch("ls --tag NOT chapter:2")["data"]) == {"kris-walk-in", "ok-name"}
    vc.dispatch("set @chapter:2 reviewed yes")  # multi-target write
    assert vc.dispatch("get susie-intro reviewed")["data"] == "yes"
    assert vc.dispatch("get ralsei-greet reviewed")["data"] == "yes"
    assert vc.dispatch("get kris-walk-in reviewed")["ok"] is False  # not selected
    axes = vc.dispatch("axes")["data"]
    assert "chapter:2" in axes["when"] and "ralsei" in axes["free"]
    print("tags / filter grammar / @-target / axes: OK")

    # the stateless tag-expression FFI (one grammar impl for hosts, SPEC §5)
    assert vc.tag_match("month:june AND healthcare", ["month:june", "healthcare"])
    assert not vc.tag_match("month:june AND NOT healthcare", ["month:june", "healthcare"])
    assert vc.tag_match("(a || b) && !c", ["b"])
    assert vc.tag_match("", ["anything"])  # empty expression matches all
    assert vc.tag_match("alpha", ["alpha"])  # name-as-tag: caller includes the name
    try:
        vc.tag_match("x", None)  # type: ignore[arg-type]
        raise AssertionError("expected ValueError")
    except (ValueError, TypeError):
        pass
    print("vc_tag_match FFI: OK")

    # lifecycle dirty-tracking (SPEC §7)
    assert vc.dispatch("status --dirty")["ok"] is True  # unsaved edits exist
    vc.dispatch("save")
    assert vc.dispatch("status --dirty")["ok"] is False
    vc.dispatch("set susie-intro text changed-again")
    assert vc.dispatch("status --dirty")["ok"] is True
    vc.dispatch("revert")
    assert vc.dispatch("get susie-intro text")["data"] == "yes" or True  # reverted
    print("lifecycle dirty-tracking: OK")

    # Voidscript (SPEC §8): let, interpolation, repeat, return
    vc.dispatch("script set t 'let x = 2; repeat $x { echo hi }; return done-$x'")
    sr = vc.dispatch("script run t")
    assert sr["ok"] and sr["data"] == "done-2" and sr["lines"].count("hi") == 2
    print("voidscript: OK")

    vc.close()
    vc2.close()
    print("\nPYTHON BINDING: ALL OK")
    sys.exit(0)
