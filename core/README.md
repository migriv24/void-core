# Void Core — C core (v0.2.0)

The **C implementation** of Void Core, exposing a pure **C ABI** so any
language can bind to it. This supersedes the JS prototype in `../src/` (kept as a
conformance oracle). Governing docs: `../SPEC.md` (contract), `../okf/design/` (design).

> **Status: the core is substantially complete** (v0.2.0, 2026-07-03; first built
> 2026-06-14 on the toolchain in `../local_compilation_discovery.md`). All five
> Void Core parts (data model, dispatcher, tag system, Voidscript,
> logging+adapter seam) are implemented and tested from both C and Python;
> `../conformance/` (SPEC §11) passes 8/8. The notable deferrals are deliberate,
> not gaps: the interaction-net **rule reducer** lives at the Python seam
> (`../reduce/`), and the advanced Voidscript constructs (def/try/include)
> remain oracle-only. See below.

## Design decisions baked in
- **cJSON is the canonical in-memory model.** The state document (SPEC §2) *is* the
  live representation, so round-tripping is free and `content` stays opaque to the
  core (SPEC §3.2, §11). cJSON is vendored (`vendor/`, MIT) and kept internal
  (`CJSON_HIDE_SYMBOLS`) — only `vc_*` symbols are exported.
- **C++ allowed later, C ABI forever.** The public header (`include/voidcore.h`) is
  pure C; the implementation could move to C++ without changing the boundary.
- **Exception-free, NULL-tolerant boundary.** No bad input from a host ever crashes
  the library; it returns an `ok:false` result. This is what makes the ABI safe to
  call from Python/Node/etc.
- **All I/O is the host's job (a "holiday").** The core does no file/network/stdout
  I/O and owns no REPL — `vc_dispatch(cmd) -> result JSON` is the whole surface.

## Module map (modular by design)
```
core/
  include/voidcore.h     # the public C ABI (the only thing hosts include)
  src/
    vc_internal.h        # shared internal surface (not exported)
    vc_manager.c         # public ABI impl + state document + host callbacks
    model/
      spirit.c           # identity: frozen id + editable name (SPEC §3.1)
      rune.c             # the atomic editable unit (SPEC §3.2)
      mantle.c           # runes over a domain (SPEC §3.4)
      binding.c          # cross-mantle bindings + ref resolution (SPEC §3.6)
    glyph/glyph.c        # the glyph registry (SPEC §3.3)
    tags/tag.c           # tag membership + filter grammar + axes (SPEC §5)
    dispatch/
      args.c             # THE §6.1 quote automaton + argv tokenizer/joiner
      codec.c            # the §6.1 codec, exported (quote / split / transcript)
      dispatch.c         # the one command router -> {ok,lines,data} (SPEC §6,§7)
      undo.c             # undo/redo snapshots (SPEC §6)
      journal.c          # the command journal: reified commands (SPEC §6.2)
      lifecycle.c        # _baseline dirty-tracking (status/diff/revert)
    scripts/voidscript.c # the Voidscript interpreter (SPEC §8)
    util/
      ids.c              # real-ID minting via OS CSPRNG (SPEC §3.1)
      log.c              # the logging spine (SPEC §9)
  vendor/cJSON.{c,h}     # vendored JSON (MIT)
  tests/smoke.c          # C end-to-end smoke
```

## Build
```bash
cmake -S core -B core/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build core/build
core/build/bin/vc_smoke.exe          # C smoke test
python bindings/python/voidcore.py   # Python ctypes binding + smoke
```
Outputs `core/build/bin/libvoidcore.dll` — self-contained (depends only on
KERNEL32 + the system UCRT; see the discovery doc).

## Public ABI (`include/voidcore.h`)
```c
VC_Manager *vc_create(const char *state_json);   // NULL/malformed => empty state
char       *vc_dispatch(VC_Manager *, const char *command); // -> {ok,lines,data} JSON
char       *vc_export_state(VC_Manager *);        // -> state document JSON
void        vc_free_str(char *);                  // free returned strings
void        vc_destroy(VC_Manager *);
const char *vc_version(void);
int         vc_tag_match(const char *expr, const char *tags_json); // SPEC §5, stateless
char       *vc_arg_quote(const char *value);            // §6.1 encode, stateless
char       *vc_argv_split_json(const char *line);       // §6.1 decode, stateless
char       *vc_transcript_split_json(const char *src);  // §6.1 decode a transcript
void        vc_set_undo(VC_Manager *, int enabled);     // §6 undo, ON by default
void        vc_set_undo_depth(VC_Manager *, int depth);  // §6 stack bound (200)
void        vc_set_journal(VC_Manager *, int enabled);  // §6.2 record, off by default
char       *vc_export_journal(VC_Manager *);            // -> reified commands JSON
void        vc_journal_clear(VC_Manager *);
```
Any returned `char*` is freed with `vc_free_str`. A `VC_Manager` is **not
thread-safe** — serialize calls per manager (SPEC §6); stateless functions
(`vc_tag_match`, the three codec calls, `vc_alloc_str`, `vc_free_str`, `vc_version`)
are safe anywhere.

## Verbs implemented
Read: `version help glyphs mantles where rune(ls) ls find describe get cat tree
validate axes status diff history log bindings links related export`
Mutate: `mantle new` · `use` · `rune new|rm|rename|move|dup` · `set` · `setjson` · `facet` · `tag` ·
`link` · `unlink` · `relate` · `unrelate` · `rule add|ls|rm|clear` · `undo` · `redo` · `revert` ·
`batch` · `bind` · `unbind`
System: `config [get <k> | set <k> <v>]` — session/host meta (`state.config`), scalar-coerced
(true/false/number/string), **outside the undo slice** (a live `config set` never touches
undo history)
Lifecycle / seam: `save` · `deploy` · `build` · `preview` · `effect <op> [args...]`
(all route through the host effect handler) · `log`
Scripts: `script run|ls|show|new|set`
POSIX aliases (SPEC §7.1, argument-aware desugarings in `args.c`): `cd`→`use` ·
`pwd`→`where` · `rm <ref>`→`rune rm <ref>` · `mv`→`rune rename` · `cp`→`rune dup` ·
`mkdir`→`mantle new` · `grep`→`find` · `man`/`?`→`help` · `quit`→`exit` ·
`dump`→`export`. Root-`ls` (no active mantle) lists mantles; `cd /` deactivates.

Any verb taking `<ref>` also accepts `@<filter>` to target a *group* of runes
(quote it if it contains spaces): `set "@chapter:2 AND NOT ralsei" reviewed yes`.

`setjson <ref> <field> <json>` sets a **typed/structured** content value
(number/bool/array/object; invalid JSON → string) — how a host/UI sets non-string
content through the dispatcher. The arg tokenizer supports `\'` inside single
quotes so arbitrary text/JSON (apostrophes, `\c` codes) passes safely. *(Both added
2026-06-15, driven by building Fountain — see the Deltarune mod tool.)*

## ABI (the full surface)
```c
VC_Manager *vc_create(const char *state_json);
char       *vc_dispatch(VC_Manager *, const char *command);
char       *vc_export_state(VC_Manager *);
char       *vc_alloc_str(const char *s);   // for host effect handlers' returns (§9)
void        vc_free_str(char *);
void        vc_destroy(VC_Manager *);
const char *vc_version(void);
int         vc_tag_match(const char *expr, const char *tags_json); // §5 grammar, stateless
int         vc_register_glyph(VC_Manager *, const char *glyph_json);
void        vc_set_log_sink(VC_Manager *, VC_LogFn, void *user);       // §9
void        vc_set_effect_handler(VC_Manager *, VC_EffectFn, void *user); // §9 holiday
char       *vc_arg_quote(const char *value);            // §6.1, stateless
char       *vc_argv_split_json(const char *line);       // §6.1, stateless
char       *vc_transcript_split_json(const char *src);  // §6.1, stateless
void        vc_set_undo(VC_Manager *, int enabled);      // §6 undo, on by default
void        vc_set_undo_depth(VC_Manager *, int depth);  // §6 frames kept (200)
void        vc_set_journal(VC_Manager *, int enabled);  // §6.2 command journal
char       *vc_export_journal(VC_Manager *);            // §6.2 the record
void        vc_journal_clear(VC_Manager *);             // §6.2
```
Glyphs and the two callbacks are host config — NOT in the exported state; set them
after each `vc_create`. The **effect handler** is the holiday boundary: the core
does its model-side work (e.g. `save` snapshots the baseline) and calls the host
for the real I/O (write files, deploy, build, preview).

### Undo control (§6)

Undo is on by default and every mutating command snapshots the whole undoable
slice (`mantles` + `active`) before it runs. That memento is cheap for a
**document** and expensive for a **world**, and which one lives in `mantles` is a
decision only the host has made. Void Unity's rule is *a thing is a rune iff it
can be the endpoint of a link*, which makes every door, crate and NPC in a
streaming map a rune; they measured 27.6 ms for one `set` at 4 000 runes —
longer than a 60 Hz frame — and quadratic construction, since each `rune new`
copies every rune already present (2026-08-28).

So `vc_set_undo(m, 0)` exists, mirroring `vc_set_journal`. Off means no frame is
taken, `undo`/`redo` fail *saying they are disabled*, and `history` says so too.
`batch` stays atomic either way — it rolls back from its own saved copy, not from
the undo stack. `vc_set_undo_depth(m, n)` bounds the stacks (default 200) and
trims immediately when lowered. Measured here, building 600 runes: **498 ms on /
18 ms off**; one `set` at 600 runes: **1 808 µs on / 8 µs off**. That is what lets
one process hold an *authoring* manager (undo on, journal on, attributed) and a
*world* manager (both off) side by side.

This is a switch, not a fix — the memento is still O(slice) when it is taken. The
fix is reified commands (`okf/design/command-architecture.md`), which two hosts
have now asked for. `bindings/python/undo_control_test.py` pins the contract.

### The §6.2 command journal

Undo is a before-image — the right structure for taking a change back here, and the
wrong one for describing a change to anything else. `vc_set_journal` turns on the
other half: each successful mutating command reified as
`{seq, command, verb, who, pure, slice, minted}`, readable through
`vc_export_journal` or the `journal` verb. **Off by default**, so a host that does
not want the record pays neither the entries nor the id-diff that fills `minted`.

`pure` is false iff the verb could reach the effect handler (`save`, `deploy`,
`build`, `preview`, `effect`) — statically, so two peers classify the same command
identically even when only one has a handler registered. `minted` is the ids that
exist after the command and did not before, which is what lets the *entry* replay
deterministically where the command *text* cannot (ids come from the CSPRNG).

### The §6.1 codec

A dispatcher argument carries an arbitrary **NUL-free byte string**, and
`split(quote(v)) == [v]` for every such `v` — newlines and quotes and backslashes
included. Both halves are exported because §6.1 spent a release as a specification
standing in for a component, and five independent implementations of it were wrong,
this one among them: the Voidscript statement reader, the condition lexer and the
interpolator each carried their own quote tracking, none of which implemented the
`'` escape, and a correctly-quoted value could therefore execute as commands
(`13-transcript-safety.vs`). There is now exactly one quote automaton in the tree
(`vc_quote_step`); a second one anywhere is a bug.

`vc_transcript_split_json` is the **decoder for a whole transcript** — the function
a host needs to review a proposed run before dispatching it. It cuts on newline and
`;` *outside* quoted runs, drops comments, returns each statement's `argv`, refuses
an unterminated quote (§6.1 rule 5 is an error since 0.2.7), and reports whether the
transcript is `flat` — no blocks, no §8 control words — which is the property that
makes its effect readable without simulating it.

## What's implemented (all five Void Core parts)
- **Data model** (SPEC §3): spirit/rune/glyph/mantle/domain/binding over cJSON;
  the state document round-trips for free (§2/§11).
- **Dispatcher** (SPEC §6/§7): one router, `{ok,lines,data}`, exception-free,
  undo/redo/history (bounded memento), dirty-tracking vs `_baseline`
  (status/diff/revert/save), atomic `batch` (one undo frame, rollback on failure).
- **Tag system** (SPEC §5): membership (tags + name + `glyph:<name>`), the full
  filter grammar (`AND/OR/NOT`, `&&/||/!`, parens, implicit-AND), `ls --tag`,
  `@<filter>` group-targeting, the `axes`, `find`, **and** the per-mantle weighted
  tag graph (`relate`/`related`/`unrelate`).
- **Voidscript** (SPEC §8): `let`, `$var`/`${var}`/`$1..`/`$@`/`$?`, `$(cmd)`
  capture, `if/elif/else`, `while`, `repeat`, `foreach v in (cmd)`,
  `break/continue`, `return`, `halt`, `assert`, operators; run via `script`.
- **Logging + adapter seam** (SPEC §9): in-memory log ring + `vc_set_log_sink`;
  `save/deploy/build/preview` route through `vc_set_effect_handler`.

## Host gotchas

Things that will look like Void Core broke your build and are not Void Core.

* **A version floor compared as a string breaks at 0.2.10.** `vc_version()` returns a
  dotted string, and `version >= "0.2.4"` is a *lexicographic* compare: `"0.2.10" <
  "0.2.4"`, because `"1"` sorts before `"4"`. Such a check is wrong from the day it is
  written and can only ever fire once a component reaches double digits, which the minor
  did at 0.2.10. Compare component-wise. (Reported by Void Maiz, 2026-08-29, who hit it
  in `embed_smoke` and asked that it be written down once for the hosts behind them.)

* **`related` is about TAGS; `links` is about EDGES.** `link a b` writes
  `layout.edges`; `relate x y` writes `tags[x].near`. They are different graphs and
  neither verb reads the other's. Because a rune's name doubles as a tag, `related
  <rune>` is accepted and answers about tag proximity — so after `link`, `related`
  correctly reports nothing. Use `links <ref>` to see edges. Since 0.2.13 the empty
  answer says so itself when the ref names a rune that has edges, rather than only
  `(no neighbors)`. (Reported by Void Hormiga, 2026-09-02, after a field agent read
  the empty answer as a failed `link` and wrote the link twice.)

## Deliberately deferred (research-track, not gaps)
- **The interaction-net rule reducer.** Not in the C library *by design*: the
  executor is built at the Python seam (`../reduce/`, the `reduce` verb — see
  `../okf/concepts/reduce.md`); in C, rules and the weighted tag graph are
  *stored and inspected* (`../okf/design/interaction-nets-theory.md`).
- **Glyph host *callbacks*** (`describe`/`newContent`/`render` in the host
  language) — needs an FFI callback bridge; glyphs are data descriptors for now.
- **Advanced Voidscript**: `def`/functions, `try/catch`, `on error`, `include`,
  `call`, `wait`, `prompt`.
- The **command-architecture** question (reified commands) —
  `../okf/design/command-architecture.md`.

First app co-developed on this: the **Deltarune mod creation tool** ("Fountain",
Python, via the ctypes binding), which hardened the core; **Hormiga** now embeds
it in production (vendored runtime + Void Console), with the Portfolio Manager
and FaultSack alongside. Conformance: `../conformance/` (SPEC §11) runs against
every build; CI publishes prebuilt win/mac/linux libraries on `v*` tags.
