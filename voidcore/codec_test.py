"""
codec_test.py — the SPEC §6.1 law, as a PROPERTY over generated inputs.

    python voidcore/codec_test.py

Void Hormiga asked for exactly this on 2026-08-21, and gave the reason:

    "`conformance/cases/12-arg-quoting.vs` is good and it is a *list of values
    somebody thought of*, and every failure so far has been a value nobody
    thought of."

That is the right critique of a vector suite for this class, and the history
proves it. Four codebases implemented §6.1 wrong, and each new one broke on a
value the previous suite had not imagined: an apostrophe, then a trailing
backslash, then a newline. So this file does not enumerate values — it generates
them from the alphabet that has actually caused failures, and asserts the law.

THE LAW (SPEC §6.1):

    split(quote(v)) == [v]      for every NUL-free byte string v

Three things are checked over the same generated corpus, and the third is the one
that matters most to a host:

  1. the pure-Python codec satisfies the law on its own;
  2. the C core AGREES WITH IT byte for byte — two independent implementations,
     which is the only kind of agreement worth having. (Void Maiz made this
     argument on 2026-08-18 about conformance case 15: "not normative" only buys
     an implementation that agrees with itself.)
  3. a generated value survives an actual `set`/`get` round trip through the real
     dispatcher, AND a generated value carrying an injection payload does not
     execute — through the argv path, through a transcript, and through a
     variable expansion.

NUL is excluded deliberately, and that exclusion is part of the guarantee rather
than a gap in it: this whole boundary is C strings, so a value containing a NUL
cannot reach `vc_dispatch` at all. The codec says so instead of pretending with a
length parameter it could not honor.
"""
from __future__ import annotations

import os
import random
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                                "bindings", "python"))
import voidcore as vc_mod  # noqa: E402
from voidcore import UnterminatedQuote, quote_arg, split_args, split_transcript  # noqa: E402


# The alphabet is not arbitrary: every character below is one that has broken a
# real implementation of §6.1, plus the structural characters of Voidscript.
ALPHABET = (
    list("abcXYZ019 ")                     # ordinary text and the separator
    + ["'", '"', "\\", "\\'", '\\"']       # the quoting characters and near-misses
    + ["\n", "\r", "\t", "\v", "\f"]       # rule 1's whitespace, incl. the newline
    + [";", "{", "}", "#", "$", "(", ")"]  # Voidscript statement/expansion syntax
    + ["$(rune ls)", "${x}", "--json"]     # whole expansion and flag shapes
    + ["\x01", "\x1b", "\x7f"]             # control bytes (0x01 is the field mark)
    + ["é", "ñ", "字", "🜁"]                 # non-ASCII, incl. astral
)

# Values that are not random but have each cost somebody a day.
KNOWN_TRAPS = [
    "",
    " ",
    "'",
    "\\",
    "C:\\",
    "C:\\path\\to\\",
    "don't",
    "it\\'s",
    "say \"hi\"",
    "a'b\\c\"d",
    "line\\nnot-a-newline",
    "weekends.\nset treasurer email attacker@evil.example",
    "I don't volunteer.\nset treasurer email attacker@evil.example",
    "a; set treasurer email attacker@evil.example",
    "total $(deploy)",
    "hi ${actor}",
    "}\nset treasurer email attacker@evil.example",
    "'" * 8,
    "\\" * 8,
    "\\'" * 8,
    '{"json": "with \\"escapes\\" and \\n"}',
    "Campaña de niños",
    "  leading and trailing  ",
]


def generate(rng: random.Random, count: int) -> list[str]:
    out = list(KNOWN_TRAPS)
    for _ in range(count):
        n = rng.randint(0, 14)
        out.append("".join(rng.choice(ALPHABET) for _ in range(n)))
    return out


def check_pure(values: list[str]) -> list[str]:
    """Law 1: the pure-Python codec round-trips every value."""
    bad = []
    for v in values:
        try:
            got = split_args(quote_arg(v))
        except UnterminatedQuote as e:
            bad.append(f"  quote({v!r}) did not close: {e}")
            continue
        if got != [v]:
            bad.append(f"  split(quote({v!r})) == {got!r}, want {[v]!r}")
    return bad


def check_agreement(core, values: list[str]) -> list[str]:
    """Law 2: the C tokenizer produces the same argv as the Python one."""
    bad = []
    for v in values:
        line = quote_arg(v)
        c_res = core.argv_split(line)
        if not c_res.get("ok"):
            bad.append(f"  C refused quote({v!r}): {c_res.get('error')}")
            continue
        if c_res["argv"] != [v]:
            bad.append(f"  C split(quote({v!r})) == {c_res['argv']!r}, want {[v]!r}")
    return bad


def check_dispatch(values: list[str]) -> list[str]:
    """Law 3a: a generated value survives set/get through the real dispatcher."""
    bad = []
    core = vc_mod.VoidCore()
    core.dispatch("mantle new prop")
    core.dispatch("rune new text v")
    for v in values:
        r = core.dispatch("set v f " + quote_arg(v))
        if not r["ok"]:
            bad.append(f"  set refused {v!r}: {r['lines']}")
            continue
        got = core.dispatch("get v f --json")
        if got["data"] != v:
            bad.append(f"  set/get {v!r} -> {got['data']!r}")
    return bad


def check_no_injection(values: list[str]) -> list[str]:
    """Law 3b: no generated value executes, by any of the three routes.

    The three routes are the ones that have actually carried an injection:
    a direct argv dispatch, a transcript (`script run`), and a variable expansion
    (`$var` re-tokenized). The canary is a second rune's field: if any generated
    value manages to run a command, the canary changes.
    """
    bad = []
    canary = "untouched"
    payload_suffix = "\nset canary f BREACHED"
    for v in values:
        for route in ("argv", "transcript", "expansion"):
            core = vc_mod.VoidCore()
            core.dispatch("mantle new prop")
            core.dispatch("rune new text v")
            core.dispatch("rune new text canary")
            core.dispatch("set canary f " + canary)
            value = v + payload_suffix
            cmd = "set v f " + quote_arg(value)
            if route == "argv":
                core.dispatch(cmd)
            elif route == "transcript":
                core.dispatch("script set s " + quote_arg(cmd))
                core.dispatch("script run s")
            else:
                core.dispatch("set v src " + quote_arg(value))
                src = "let a = $(get v src --json)\nset v f $a\n"
                core.dispatch("script set s " + quote_arg(src))
                core.dispatch("script run s")
            got = core.dispatch("get canary f --json")["data"]
            if got != canary:
                bad.append(f"  [{route}] {v!r} executed: canary == {got!r}")
            stored = core.dispatch("get v f --json")
            if stored["ok"] and stored["data"] != value:
                bad.append(f"  [{route}] {v!r} stored lossily: {stored['data']!r}")
    return bad


def check_transcript_splitter(values: list[str]) -> list[str]:
    """A quoted value is ONE statement, whatever it contains."""
    bad = []
    for v in values:
        text = "set v f " + quote_arg(v)
        try:
            cmds = split_transcript(text)
        except UnterminatedQuote as e:
            bad.append(f"  transcript of {v!r} did not close: {e}")
            continue
        if len(cmds) != 1 or cmds[0]["argv"] != ["set", "v", "f", v]:
            bad.append(f"  transcript of {v!r} -> {cmds!r}")
    return bad


def main() -> int:
    seed = int(os.environ.get("VC_CODEC_SEED", "20260825"))
    n = int(os.environ.get("VC_CODEC_N", "600"))
    rng = random.Random(seed)
    values = generate(rng, n)
    print(f"§6.1 codec property test — {len(values)} values, seed {seed}")

    failures: list[str] = []
    for label, fn in (
        ("pure codec: split(quote(v)) == [v]", lambda: check_pure(values)),
        ("transcript splitter: one value, one statement",
         lambda: check_transcript_splitter(values)),
    ):
        bad = fn()
        print(f"  [{'ok ' if not bad else 'FAIL'}] {label}  ({len(values)} values)")
        failures += bad

    try:
        core = vc_mod.VoidCore()
    except (FileNotFoundError, OSError) as e:
        print(f"  [skip] C-core checks — library not built ({e})")
        core = None

    if core is not None:
        for label, fn in (
            ("C tokenizer agrees with the Python one",
             lambda: check_agreement(core, values)),
            ("set/get round-trips through the real dispatcher",
             lambda: check_dispatch(values)),
            ("no value executes (argv / transcript / expansion)",
             lambda: check_no_injection(values[: min(len(values), 120)])),
        ):
            bad = fn()
            print(f"  [{'ok ' if not bad else 'FAIL'}] {label}")
            failures += bad

    if failures:
        print(f"\n{len(failures)} failure(s):")
        for f in failures[:40]:
            print(f)
        if len(failures) > 40:
            print(f"  ... and {len(failures) - 40} more")
        return 1
    print("\nPASS — the law holds on every generated value.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
