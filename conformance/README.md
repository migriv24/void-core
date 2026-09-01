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

**Cases are read as bytes and decoded without newline translation** (SPEC §11), and
any runner in another language MUST do the same. This is not fastidiousness: the
runner used to read cases in Python text mode, whose universal-newline translation
rewrote CRLF to LF before the library saw a byte — so no case could observe how an
implementation treats a CR, while `14-journal.vs` sat in the repository *with* CRLF,
green here and red for the first host that read it faithfully (Void Unity, 2026-08-27).
A suite that normalizes its own inputs is not testing what a host will be handed.
`.gitattributes` keeps `*.vs` at LF, with `15-crlf.vs` marked `-text` because it is the
one case whose bytes are the point.

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
| `09-config.vs` | §7 | the `config` verb (get/set/list), scalar coercion, and its isolation from the undo slice (regression: VLS handoff 2026-07-06) |
| `10-place.vs` | §3.2, §6, §7.2, §9 | `place` and the view slice — on the mutation spine, no undo frame, undo/redo overlays surviving runes' placements, failed `batch` still rolls back |
| `11-mantle-lifecycle.vs` | §3.4, §7.1, §7.2 | `mantle rm`/`rename` (+ `rmdir`), rm-of-active deactivates, name reuse after rm, undo restores mantle + runes + active |
| `12-arg-quoting.vs` | §6.1 | argument quoting — quote-stripping, the single `\'` escape, strip-anywhere quoting, literal backslashes, and the trailing-backslash trap — whose output is now REFUSED (rule 5) rather than silently truncating a value |
| `13-transcript-safety.vs` | §6.1, §8.1 | a value in a transcript is DATA, never syntax — newline/`;`/`}` inside a quoted argument, `'` not closing the statement, single quotes suppressing `$` expansion, an expansion staying exactly one argument (including empty), and `${var}` as an expansion rather than a block |
| `14-journal.vs` | §6.2 | the command journal — off by default, `minted` ids, `pure`/`slice` classification, canonical `command`, failed commands recording nothing |
| `15-crlf.vs` | §8 | **stored with CRLF on purpose** — a CR is a line terminator, not content: values don't carry it, string comparison sees through it, and a line ending inside a quoted run is still data |
| `16-validate-endpoints.vs` | §3.7, §7.2 | `validate` classifies an unresolved link endpoint — **cross-kind** (names a mantle) vs **dangling** (names nothing), each side, both at once, and the classification following the state document rather than being stored on the edge |

Not covered here (tested elsewhere or host-dependent): the §9 adapter/effect seam
(`bindings/python/effect_test.py`), §9 attribution + the mutation spine
(`bindings/python/attribution_test.py`), the dispatcher's **[seam]** integration of the
transformation verbs (`voidcore/dispatch_test.py` and siblings, per §11), script
arguments `$1`/`$@` (inline `script set` interpolates `$` before the source is stored),
and the §8 advanced constructs (oracle-only).

## The transformation layers

The three transform layers (SPEC §7 `[seam]`) live in Python, outside the C core, so a
host in another language has to implement them itself. Each therefore has its own
language-neutral contract + pure-JSON cases, in the same shape: a `README.md` stating
the semantics, `cases/*.json`, and a small portable `run.py` (~100 lines) to port.

| suite | layer | run |
|---|---|---|
| [`reduce/`](reduce/README.md) | the interaction-net executor (+ **composition** — a mantle as a rune inside another mantle, §7 — and the `patch` content rule, §2) | `python conformance/reduce/run.py` |
| [`temper/`](temper/README.md) | normalization (idempotent, context-blind) | `python conformance/temper/run.py` |
| [`scry/`](scry/README.md) | projection (the read side) | `python conformance/scry/run.py` |

Each suite's runner also checks that layer's **laws** on every case, not only where a
case asks: confluence + schedule-independent identity for Reduce, idempotence + purity
for Temper, purity for Scry. An implementation that matches every expected output but
breaks a law is not conforming, and the runner says so by name.

> These exist because a layer without a contract gets **reinvented rather than ported**.
> Void Maiz ported Reduce against `reduce/` and verified it; with no equivalent for the
> other two, Maiz rebuilt Scry and Void Hormiga rebuilt Temper from the concept pages.
> Two hand-written copies of a normalization pass drift, and without case files the
> drift is silent. (Observed by Void Palabra, 2026-07-27.)
