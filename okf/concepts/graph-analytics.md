---
type: Concept
title: Graph analytics
description: Deterministic graph-metric tools (centrality, clustering, bridges) over any Void Core graph, exposed to agents — realized as a compute holiday.
resource: holidays/graph/analytics.py
tags: [status:current, audience:dev, audience:library, confidence:verified]
timestamp: 2026-06-18T00:00:00Z
---

Because a [mantle](/concepts/mantle.md) is a graph of [runes](/concepts/rune.md)
joined by [links](/concepts/links.md) (and a consumed
[OKF](/components/okf-engine.md) bundle is a mantle), Void Core can offer
**graph-analysis tools** that apply uniformly to *any* mantle — not just OKF.

# What

- **Centrality** — degree, betweenness, eigenvector / PageRank. "Which concept is
  most central? Which is a bridge?"
- **Community detection** — clusters of tightly-linked concepts.
- **Paths & reach** — shortest path, neighborhood, orphans/dangling targets.

# Why it matters for agents

These turn blind traversal into guided maintenance: an agent updating an OKF can ask
*which doc has high betweenness* (edit carefully — many things route through it), or
*which cluster is all `status:stale`* (refresh together). The same metrics inform
context-length optimization (prioritize central concepts) — see [context optimization](/design/context-optimization.md).

# Placement (keeps the core minimal)

The pure [C core](/components/c-core.md) only *emits the graph*; the algorithms run
in a **compute [holiday](/concepts/holiday.md)** (host-side — e.g. Python `networkx`
or Rust `petgraph`), surfaced as dispatcher verbs so the CLI and agents share them.
Heavy algorithms never enter the core.

# Lightweight NLP (further out, optional)

Semantic link-*suggestion* and concept-finding across markdown — useful for proposing
[links](/concepts/links.md) and for context optimization — are an **optional
embedding/LLM holiday**, not a core feature. Heaviest, least-generalizable piece;
design toward it, build it last. Link *creation* stays explicit (markdown links) +
the deterministic tools above first.

# Status

`current` for the **deterministic** tools (verified 2026-06-18): a pure-Python,
dependency-free [holiday](/concepts/holiday.md) at `holidays/graph/` — degree,
in/out-degree, **betweenness** (Brandes), **PageRank** (power iteration), **label-
propagation communities**, and connected components, validated against known-value
graphs. Surfaced via `python holidays/okf analyze` and fed into the viewer (betweenness-
ranked "most central", community per concept). The optional **NLP** layer (semantic
link-suggestion, context optimization) remains `planned`. Still `planned` too:
exposing these as [dispatcher](/concepts/dispatcher.md) verbs in the core itself.
