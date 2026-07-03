# Void Core

A host-agnostic engine other applications build on: one command **dispatcher**
(CLI / GUI / script runner all call it, every mutation undoable), the
rune/mantle/domain data model, an axis-typed **tag system** with a filter
grammar, the **Voidscript** runner, a logging spine, and an adapter/effect seam
("holidays") where all real I/O lives. No LLM embedded — architected so agents
drive it through the dispatcher surface.

The reference implementation is the **C core** (`core/`, ~3.5k lines of plain
C11 + vendored cJSON) exposing a pure C ABI (`core/include/voidcore.h`), so any
language binds to it. A Python ctypes binding (`bindings/python/voidcore.py`) is
the primary consumption path today; the JS prototype (`src/`) is kept as the
conformance oracle. Current version: **0.2.0**.

**Standalone project.** Its own git repo; applications (Hormiga, Portfolio
Manager, Fountain, FaultSack) vendor or locally install it and are developed in
their own repos — Void Core stays isolated.

> **Agents & contributors: start at [`okf/index.md`](okf/index.md)** — the
> [Open Knowledge Format](okf/references/okf-spec.md) bundle that describes Void
> Core itself (concepts, components, design rationale, roadmap), with `status:`
> honesty on what is built vs planned. It is the self-describing map of the project.

Documents, by role:
- **[`okf/`](okf/index.md)** — the knowledge bundle. Start here to understand the system.
- **`SPEC.md`** — the normative, language-agnostic contract (what any implementation
  must do); indexed from the OKF at [`okf/references/spec.md`](okf/references/spec.md).
- **[`conformance/`](conformance/README.md)** — language-neutral SPEC §11 test cases
  (self-checking Voidscript scripts) every implementation runs.
- `ARCHITECTURE.md` — the JS prototype's design narrative (the conformance oracle).
- The C core lives in [`core/`](core/README.md); design rationale is in
  [`okf/design/`](okf/design/index.md) (the absorbed research notes).

## Get the library

Build locally (any platform with CMake + a C compiler):

```bash
cmake -S core -B core/build -G Ninja
cmake --build core/build          # -> core/build/bin/libvoidcore.dll / .so / .dylib
python conformance/run.py         # SPEC §11 suite against the fresh build
```

Or take a prebuilt library from a **GitHub Release**: every `v*` tag publishes
`libvoidcore-win-x64.dll`, `libvoidcore-macos-universal.dylib`, and
`libvoidcore-linux-x64.so`, each built and conformance-tested by CI
(`.github/workflows/ci.yml`). Downstream apps vendor a pinned runtime.

## Use it from Python (the main path)

```python
from voidcore import VoidCore          # bindings/python/voidcore.py

vc = VoidCore()                        # or VoidCore(state=<state document>)
vc.register_glyph({"glyph": "note", "label": "Note", "editor": "form", "fields": ["text"]})
vc.set_effect_handler(lambda op, args: ...)   # save/deploy/build/preview + `effect <op>`

vc.dispatch("mantle new demo")
vc.dispatch("rune new note hello")
vc.dispatch('set hello text "world"')
names = vc.dispatch("ls")["data"]      # every verb returns {ok, lines, data}

vc.tag_match("month:june AND event", ["month:june", "event"])  # the SPEC §5
# grammar over the FFI — one impl; hosts filtering holiday/external entities
# call this instead of reimplementing it.
```

The transformation verbs (`scry` / `temper` / `materialize` / `reduce`) come from
the seam superset `voidcore.Dispatcher` (repo-root `voidcore/` package), which
delegates everything else to the C core unchanged. See `SPEC.md` §7.

## Use the JS oracle as a CLI

```bash
npm install
node src/cli/cli.js --state ./demo/state.json            # interactive REPL
node src/cli/cli.js --state ./demo/state.json --json ls  # machine output
```

## The model in one breath

- **rune** — atomic editable unit: a `spirit` (frozen real-ID + human name), six
  `facets` (who/what/when/where/why/how), a `glyph` (how it's edited), `content`,
  `tags`.
- **mantle** — a group of runes over a domain, plus a layout graph + rule set.
- **domain** — the real hosting target (repo, build/deploy/preview commands, port).
- **tag** — organizational metadata; a rune's name doubles as a tag.
- **holiday** — the protocol seam to an external system the app doesn't own.

## Command surface

`describe ls tree get find cat status diff history glyphs axes mantles domain
validate where links · set setjson facet tag rune link unlink mantle bind
bindings unbind undo redo batch · save deploy build preview effect revert ·
scry temper materialize reduce (seam) · script log use config export import
help version exit`

**POSIX aliases** (SPEC §7.1 — argument-aware desugarings, never a semantic
fork): `cd`→`use`, `pwd`→`where`, `rm <ref>`→`rune rm <ref>`, `mv`→`rune rename`,
`cp`→`rune dup`, `mkdir`→`mantle new`, `grep`→`find`, `man`/`?`→`help`,
`quit`→`exit`, `dump`→`export`. With no active mantle, `ls` lists the mantles;
`cd /` (or bare `use`) deactivates. Mantle ≈ directory, rune ≈ file, tag
expression ≈ glob.

Run `help` in a REPL, or read `SPEC.md` §7. The scripting language (Voidscript)
is `SPEC.md` §8.

## Layout

```
core/           the C core: src/, include/voidcore.h (the C ABI), tests, CMake
bindings/       language bindings (python/voidcore.py — ctypes over the ABI)
conformance/    SPEC §11 cases (self-checking Voidscript) + run.py
voidcore/       the Python dispatcher seam (transform verbs) + spec compilers
scry/ temper/ reduce/   the three transformation layers (pure, tested)
holidays/       adapter implementations (localjson, meshdb, okf, graph)
src/            JS prototype — the conformance oracle (CLI + dispatcher)
okf/            the knowledge bundle (concepts, components, design, log)
SPEC.md         the normative contract
```
