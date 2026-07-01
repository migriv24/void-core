# Void Core — C core (v0.1.0)

The new **C implementation** of Void Core, exposing a pure **C ABI** so any
language can bind to it. This supersedes the JS prototype in `../src/` (kept as a
conformance oracle). Governing docs: `../SPEC.md` (contract), `../notes/` (design).

> **Status: the core is substantially complete.** All five Void Core parts (data
> model, dispatcher, tag system, Voidscript, logging+adapter seam) are implemented
> and tested from both C and Python. Built & verified 2026-06-14 on the toolchain
> in `../local_compilation_discovery.md`. The notable deferrals are research-track,
> not gaps: the interaction-net **rule reducer** (rules are stored, not executed,
> on purpose) and the advanced Voidscript constructs (def/try/include). See below.

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
      args.c             # quote-aware argv tokenizer (SPEC §6)
      dispatch.c         # the one command router -> {ok,lines,data} (SPEC §6,§7)
      undo.c             # undo/redo snapshots (SPEC §6)
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
```
Any returned `char*` is freed with `vc_free_str`.

## Verbs implemented
Read: `version help glyphs mantles where rune(ls) ls find describe get cat tree
validate axes status diff history log bindings links related export`
Mutate: `mantle new` · `use` · `rune new|rm|rename|move|dup` · `set` · `setjson` · `facet` · `tag` ·
`link` · `unlink` · `relate` · `unrelate` · `rule add|ls|rm|clear` · `undo` · `redo` · `revert` ·
`batch` · `bind` · `unbind`
Lifecycle / seam: `save` · `deploy` · `build` · `preview` (route through the host
effect handler) · `log`
Scripts: `script run|ls|show|new|set`

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
void        vc_free_str(char *);
void        vc_destroy(VC_Manager *);
const char *vc_version(void);
int         vc_register_glyph(VC_Manager *, const char *glyph_json);
void        vc_set_log_sink(VC_Manager *, VC_LogFn, void *user);       // §9
void        vc_set_effect_handler(VC_Manager *, VC_EffectFn, void *user); // §9 holiday
```
Glyphs and the two callbacks are host config — NOT in the exported state; set them
after each `vc_create`. The **effect handler** is the holiday boundary: the core
does its model-side work (e.g. `save` snapshots the baseline) and calls the host
for the real I/O (write files, deploy, build, preview).

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

## Deliberately deferred (research-track, not gaps)
- **The interaction-net rule reducer.** Rules and the weighted tag graph are
  *stored and inspected*, not *executed* — the "modeled as a net now, reduced
  later" discipline (`../notes/concept-brainstorm.md`,
  `../notes/interaction-nets.md`). The math (commutative-monoid effects, no
  vicious cycles) is decided; the executor is future work.
- **Glyph host *callbacks*** (`describe`/`newContent`/`render` in the host
  language) — needs an FFI callback bridge; glyphs are data descriptors for now.
- **Advanced Voidscript**: `def`/functions, `try/catch`, `on error`, `include`,
  `call`, `wait`, `prompt`.
- The **command-architecture** question (reified commands) —
  `../notes/command-architecture.md`.

First app to be built on this: the **Deltarune mod creation tool** (Python, via
the ctypes binding), co-developed to harden the core before Hormiga.
