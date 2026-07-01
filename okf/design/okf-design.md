---
type: Design
title: OKF as a core feature — design
description: Documentation as a core feature: a mantle IS an OKF bundle; dev + filtered library bundles with status honesty.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Void Core — OKF (Open Knowledge Format) engine

> A *thinking document*, not the contract (`../SPEC.md` is normative). Source
> material: Google Cloud's **Open Knowledge Format** v0.1 spec and the
> [knowledge-catalog](https://github.com/GoogleCloudPlatform/knowledge-catalog)
> reference repo (`okf/SPEC.md`, an `enrichment_agent`, sample `bundles/` each
> with a self-contained `viz.html`). Status of *this note*: the **design** is
> `status:current` thinking; the **engine** it describes is `status:planned`.
> Written 2026-06-18, revised the same day after the "engine as holiday" reframing.

## What OKF is (the parts that bind us)

A *Knowledge Bundle* is a directory tree of UTF-8 markdown files; each non-reserved
`.md` is a *Concept* whose **Concept ID is its path minus `.md`**. A concept is YAML
frontmatter + a markdown body. **Only `type` is required**; `title`/`description`/
`resource`/`tags`/`timestamp` are recommended; producers may add any keys. Concepts
cross-link with ordinary markdown links (absolute, bundle-root-relative `/…` form
preferred), forming a graph. Reserved filenames: `index.md` (directory listing,
no frontmatter except an optional `okf_version` at bundle root) and `log.md`
(dated history). Citations go under `# Citations`. **Conformance is permissive**:
consumers MUST tolerate unknown `type`s, unknown keys, missing optional fields, and
**broken links** (a dangling link is "not-yet-written knowledge," not an error).

That last rule matters for us: **broken _intra-bundle_ links are legal**; only stale
`resource:`-to-code links are a freshness signal (see §"Accuracy").

## Two reframings that drive the design

### A. Speak OKF outward, Void Core inward — bridged by a dictionary

The OKF engine's **public surface uses standard OKF vocabulary** (Concept, Concept
ID, Bundle, `type`, frontmatter, body, link, citation). Void Core's private nouns
(rune, glyph, mantle, facet) stay **internal**. We want maximum integratability:
the overwhelming majority of producers and consumers will never have heard of Void
Core, and an integration format that forces them to learn our ontology is a dead
format. A **dictionary** (below) is the seam, and it *ships as a concept in the
bundle* so the mapping is self-describing and travels with the data.

### B. The OKF engine is a holiday — and consuming beats producing

An OKF bundle that describes some external repo/app/website is a **knowledge system
Void Core does not own**. Reaching it is the holiday pattern exactly (SPEC §10.1).
So the engine is an **OKF holiday**: `kind:knowledge`, protocol = OKF over a
filesystem / git repo / tarball, with the holiday interface expressed over
*concepts*:

```
query(tagExpr)   -> [Concept]     # resolve a tag/type filter across the bundle
get(conceptId)   -> Concept        # one concept by its path-id
describe()       -> { bundle index, types present, counts, capabilities }
insert(concept)  -> conceptId      # produce/append (reverse direction)
```

**Why this ordering matters:** Void Core will be niche at first, so *most* OKF
bundles in the world will describe non-Void-Core things. The engine's first job is
therefore **consumption** — ingest, query, validate, and serve *any* conformant
bundle. Producing a bundle from a Void Core mantle is the same holiday run in
reverse, and only applies to the minority of bundles that describe a Void Core app.

## The dictionary (OKF ⇄ Void Core)

Internally the engine stores consumed concepts as runes in a mantle, so the tag
engine, filter grammar (SPEC §5), `validate`, and undo all work for free. That is an
**implementation detail behind the dictionary**, never exposed in the OKF surface:

| OKF term (public) | Void Core term (internal) | notes |
|---|---|---|
| Concept | rune | one knowledge unit |
| Concept ID (path minus `.md`) | `spirit.name` (+ stable `spirit.id`) | export derives collision-free paths from names |
| `type` (required) | a `type:<value>` tag (+ a generic `okf-concept` glyph) | OKF types are free-form; do NOT force them to be registered glyphs |
| `title` | facet — or `spirit.name` | |
| `description` / `resource` / `timestamp` | facets `what` / `where` / `when` | `resource` + `when` are the freshness hooks |
| `tags` | tags | verbatim; same field, same grammar |
| body (markdown) | `content` (opaque) | rendered by the `okf-concept` glyph |
| a markdown link between concepts | `layout.edges` | untyped directed edge |
| Bundle | mantle (over a domain = the bundle's source dir/repo) | |
| `index.md` | `describe <mantle>` (generated) | |
| `log.md` | logging spine + undo history | |
| `viz.html` | a render / output holiday | §"Viewer" |

**Decision:** consumed OKF concepts use **one generic `okf-concept` glyph** with the
OKF `type` carried as a `type:<value>` tag — *not* one glyph per OKF type. Glyphs are
registered and Void-Core-specific; OKF types are open-world and external. Keeping
them as tags honors OKF's "tolerate unknown types" rule and keeps the engine able to
ingest a bundle it has never seen.

The dictionary ships in the bundle as e.g. `references/voidcore-glossary.md`
(`type: Dictionary`) **only** when the bundle describes a Void Core app; arbitrary
external bundles need no dictionary at all.

## Honesty lives in standard OKF `tags`

The CURRENT-vs-PLANNED discipline is producer-defined tags — fully OKF-conformant,
no dialect:

| tag | values | purpose |
|---|---|---|
| `status:` | `current` / `planned` / `deprecated` | the "log it, build it later" honesty |
| `audience:` | `dev` / `library` | which projected bundle a concept belongs to |
| `confidence:` | `verified` / `asserted` / `stale` | trustworthiness; set by `validate`, not by hand |

A concept is born `status:planned` and may not claim `status:current` without a
`resource:` link to the code that backs it.

## The `roadmap.md` convention (dev bundles)

A **dev** bundle SHOULD include a `roadmap.md` — the forward-looking counterpart to
the reserved `index.md` (current listing) and `log.md` (history): an index of
`status:planned` concepts plus the intended build order. OKF v0.1 does **not** reserve
`roadmap.md`, so to stay conformant it is a **concept of `type: Roadmap`** whose body
is the index (not a bare frontmatter-less listing, which §9 would reject). It is
`audience:dev`, so the shipped library bundle (`status:current AND audience:library`)
excludes it — consuming agents shouldn't see unbuilt features. Because it mirrors
`okf query "status:planned"`, the engine can later regenerate or validate it against
the live bundle. `Roadmap` joins `Dictionary`/`Reference` as a documentation type
exempt from the "current needs a code `resource:`" honesty rule.

## Two OKFs = one mantle, two export filters

The dev / shipped split is a tag query, never two copies:

```
okf export ./okf-dev                                       # everything
okf export ./okf-lib --where "status:current AND audience:library"
```

An app *using* Void Core ships the filtered library bundle — a current-only OKF of
Void Core — without anyone maintaining a parallel file set.

## The engine's four jobs (this is the "OKF ENGINE", generalized)

1. **Consume** — ingest any conformant bundle (fs/git/tarball) → queryable concepts.
2. **Produce** — export a mantle → a conformant bundle (filtered by tag).
3. **Validate / refresh** — keep bundles "updated, correct, reliable" (§Accuracy),
   runnable repeatably (a cron/CI/agent pass), which is what makes it an *engine*
   rather than a one-shot exporter.
4. **Serve** — emit a self-contained viewer for a bundle (§Viewer).

CLI verbs (the agent surface; standard OKF nouns):
`okf import <dir|repo|tar>` · `okf export <dir> [--where <filter>]` ·
`okf get <conceptId>` · `okf ls [--type T] [--tag expr]` · `okf describe` ·
`okf validate` · `okf serve [<dir>]`.

## Agent tooling: traversal, not blind authoring

An agent should not have to author or re-traverse a whole bundle by hand. Two tooling
layers help — and both are **holidays, not core features** (the move that keeps the
core minimal while making it a universal foundation):

- **Graph analytics (deterministic, do soon).** Centrality (betweenness, eigenvector/
  PageRank), community detection, bridges, orphans — over any mantle, since a bundle
  *is* a mantle. Lets an agent ask "which concept is central (edit carefully)?" or
  "which cluster is all `status:stale` (refresh together)?". The core emits the graph;
  a **compute holiday** (host-side `networkx` / `petgraph`) computes metrics, surfaced
  as dispatcher verbs. (Bundle concept: `okf/concepts/graph-analytics.md`.)
- **Lightweight NLP (optional, later).** Semantic link-*suggestion* and concept-
  finding across markdown, feeding link proposals and context-length optimization
  (`notes/context-and-rl.md`). An **optional embedding/LLM holiday**, not core; the
  heaviest, least-generalizable piece. Keep link *creation* explicit + graph tools
  deterministic first; design toward NLP, build it last.

Leverage to remember: any graph capability the core gains, the OKF engine inherits
for free, because a bundle is a mantle is a graph.

## Accuracy — honestly

OKF does not keep itself accurate; markdown drifts from code. The engine provides
**drift detection, not self-correction**:

- **Stale-resource check.** For concepts with a `resource:` pointing at a repo file
  or symbol, compare the concept's `timestamp` to the git-mtime of that target; if
  the code is newer, set `confidence:stale`. *(This is the real freshness signal.)*
- **Dead-resource check.** A `resource:` that points at a path/symbol that no longer
  exists → flag. *(Distinct from a broken intra-bundle link, which OKF says to
  tolerate as planned knowledge — do NOT flag those.)*
- **Verification stamp.** An agent/CI pass re-reads `stale`/`asserted` concepts and
  re-stamps `confidence:verified` + a fresh `timestamp`.

Reconciliation stays a human/agent pass; the engine makes drift *detectable* and the
fix *cheap*, never automatic.

## Viewer (the index.html / viz.html feature)

Ship a **single self-contained HTML file** that renders any bundle as an interactive
concept graph — no backend, no data leaving the page — mirroring knowledge-catalog's
per-bundle `viz.html`. It is a Void Core **output holiday**: `okf serve` writes/opens
it. First test target is **Void Core's own dev bundle** — a live map of the project
for brainstorming and architecture, for both the human and agents. Honor OKF's
minimalism: borrow/clone the stock visualizer before writing our own.

## Implementation, staged (format first, factory second)

1. **Tag conventions** — `status:` / `audience:` / `confidence:` / `type:`. Zero code.
2. **Hand-author the Void Core dev bundle** — `okf/` (or `notes/okf-bundle/`), one
   concept per piece (interaction-nets, rune-as-monoid, mantle-as-graph-rewrite,
   holiday-as-BaaS, the OKF engine itself, tag system, Voidscript, dispatcher, C
   core, MeshDB holiday, Fountain), tagged honestly. The conformance fixture.
3. **Serve it** — wire the stock `viz.html` to that bundle so there's a viewer early
   (high motivational payoff; validates the format end-to-end).
4. **The OKF holiday** — consume (import/get/ls/query) first, then produce (export).
5. **Validate / refresh** — stale/dead-resource checks + the `confidence:` stamp.
6. **CLI verbs** + the dictionary concept.

## Scope honesty (what we are NOT doing yet)

- Not building the holiday / CLI verbs / validator yet (`status:planned`).
- Not auto-correcting docs — only detecting drift.
- Not inventing an OKF dialect: stay conformant to v0.1 so Void Core bundles render
  in the stock `viz.html` unchanged; our honesty vocabulary is ordinary `tags`.
- Not registering OKF types as glyphs (open-world types stay tags).
- Not merging the agent memory store into the codebase bundle (prior art, not a
  unification proposal).

## Open questions

- **Identity / path derivation.** Export must turn unique rune `name`s into stable,
  collision-free Concept IDs (paths), and reconstruct `layout.edges` from links on
  re-import. Round-trip (import→export→import) must be stable.
- **Where bundles live.** Dev bundle in-repo (`okf/`); library bundle is a build
  artifact (gitignored / released as a tarball).
- **Domain of a consumed bundle.** When ingesting an external repo's OKF, the
  "domain" is that repo — does the engine track provenance/source URL per concept?
- **Viewer sourcing.** Clone knowledge-catalog's `viz.html` (Apache-2.0) vs. write a
  minimal own. Check how it loads bundle data (inlined JSON vs. fetch) before deciding.
- **Dictionary placement.** Always a `references/voidcore-glossary.md` concept, or an
  engine-level table surfaced only on request?
