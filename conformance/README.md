# Conformance cases (SPEC §11)

Language-neutral test cases that every implementation of Void Core runs — the
`conformance/` directory SPEC §11 calls for. The format is the one the project's own
[Voidscript concept](../okf/concepts/voidscript.md) proposes: **self-checking
Voidscript scripts** (`assert`-over-dispatcher). A conforming implementation needs no
test framework to run them — only its own dispatcher and the SPEC §8 **core subset**.

## Format

Each `cases/NN-name.vs` file is a Voidscript script that:

1. builds its own fixture state through dispatcher verbs (`mantle new`, `rune new`, …);
2. asserts the behaviors it targets — a false `assert` halts with code 1 at the first
   violation, and the failing assertion text lands in `lines`;
3. ends with `return <NN-name>-ok`.

A case **passes** iff `script run` returns `ok: true` with `data == "<NN-name>-ok"`
(the `return` sentinel distinguishes "ran to the end" from "silently stopped early").
Cases use only built-in glyphs (§3.3) and the §8 core subset, so they are portable
across implementations by construction.

## Running

    python conformance/run.py              # all cases against the C core (via the Python binding)
    python conformance/run.py --dll <path> # against another build of the library
    python conformance/run.py conformance/cases/01-tags.vs   # one case

The runner delivers each script through the §2 state document (`scripts` map) and
invokes `script run` — so state-document loading is itself exercised by every case.

## Coverage

| case | SPEC | what it fixes |
|---|---|---|
| `01-tags.vs` | §5 | name-as-tag, `glyph:<name>`, AND/OR/NOT + `&&`/`||`/`!`, parens, implicit AND, case-insensitive operators, `@<expr>` multi-target |
| `02-identity.vs` | §3.1, §3.4, §4 | duplicate-name reject, rename keeps id/content, reference repointing, taken-name reject, remove |
| `03-undo-redo.vs` | §6 | undo/redo walk, redo cleared on new mutation, dirty flag |
| `04-links.vs` | §3.7 | link/unlink, dangling endpoints legal, repoint-on-rename, drop-on-remove |
| `05-voidscript.vs` | §8 | let/interpolation, if/elif/else, while, repeat, foreach, break/continue, `$?`, error contract |
| `06-batch.vs` | §6, §7 | batch atomicity (rollback on failure), one undo frame per batch |
| `07-posix.vs` | §7.1 | POSIX aliases as argument-aware desugarings (`mkdir`/`pwd`/`grep`/`man`/`cp`/`mv`/`rm`/`cd`), alias mutations undo like their canonical form, root-`ls` lists mantles, `cd /` deactivates |
| `08-capture-flags.vs` | §6, §7, §8 | regression (Hormiga handoff 2026-07-03): a trailing flag (`--json`) after a `--tag <expr>` value must not join into the tag expression — direct, `$(…)` capture, and `foreach` paths |

Not covered here (tested elsewhere or host-dependent): the §9 adapter/effect seam
(`bindings/python/effect_test.py`), the **[seam]** transformation verbs
(`voidcore/dispatch_test.py` and siblings, per §11), script arguments `$1`/`$@`
(inline `script set` interpolates `$` before the source is stored), and the §8
advanced constructs (oracle-only).
