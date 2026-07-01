# OKF engine — the Open Knowledge Format as a Void Core holiday

A Void Core **holiday** (`kind:knowledge`) over [Open Knowledge Format](https://cloud.google.com/blog/products/data-analytics/how-the-open-knowledge-format-can-improve-data-sharing/)
bundles — directories of markdown + YAML frontmatter. Design: `../../okf/design/okf-design.md`.

## The stance

- **Speaks standard OKF outward, Void Core inward.** Public vocabulary is Concept /
  Concept ID / Bundle / `type`; runes/mantles/glyphs stay internal (see the bundle's
  `references/voidcore-glossary.md`).
- **Consuming beats producing.** Void Core is niche, so most bundles describe
  non-Void-Core things. Everything here works on *any* conformant bundle — no Void
  Core required. Producing a bundle from a mantle is a later, separate slice.

## What's built (v0.1)

| file | job | notes |
|---|---|---|
| `bundle.py` | **consume** | parse a bundle → `Concept`/`Bundle` model; cross-link graph; a SPEC §5 tag-filter compiled to a Python predicate |
| `voidcore_bridge.py` | **consume / produce** | map concepts ↔ runes through the C core (glyph `okf-concept`, links → `layout.edges`); round-trips losslessly; `--where` library projection |
| `validate.py` | **validate** | OKF §9 conformance + drift detection: dead/stale `resource:`→code (filesystem mtime, day-granular), broken intra-bundle links (tolerated → info), honesty rule (`status:current` needs a code `resource:`) |
| `__main__.py` | CLI | `ls` / `get` / `query` / `validate` / `produce` / `analyze` |

A bundle is *viewed* in **FaultSack** (the dedicated OKF study tool); this engine ships
no visualizer and exposes the bundle as data (`ls`/`get`/`query`/`analyze`).

`analyze` uses the graph-analytics compute holiday (`../graph/`) over the bundle's
concept graph (a bundle is a mantle is a graph).

## Use

```bash
python holidays/okf ls --status planned          # list planned concepts
python holidays/okf query "status:current AND audience:library"
python holidays/okf get concepts/holiday --json  # one concept + links/backlinks
python holidays/okf validate                     # conformance + drift report (exit 1 on errors)
python holidays/okf analyze                       # centrality / communities (graph holiday)
python holidays/okf produce ./okf-lib --where "status:current AND audience:library"
python holidays/okf --bundle path/to/other       # any external bundle
```

`produce` consumes the bundle into the C core and writes a (optionally filtered)
bundle back out — the library projection ships the current-only OKF an app consumes.
Round-trip self-test: `python holidays/okf/voidcore_bridge.py`.

Default bundle is the repo's `../../okf/`.

## Planned

The graph-analytics holiday (real centrality/clustering), and exposing these jobs as
[dispatcher](../../okf/concepts/dispatcher.md) verbs in the core itself so the core's
CLI and agents share them (today the engine is a host-side holiday).
