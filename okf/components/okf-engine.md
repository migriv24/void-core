---
type: Engine
title: OKF engine
description: The Open Knowledge Format as a Void Core holiday — consume, produce, and validate knowledge bundles.
resource: holidays/okf/__main__.py
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

The **OKF engine** makes [Open Knowledge Format](/references/okf-spec.md) bundles a
first-class Void Core capability. It is itself a [holiday](/concepts/holiday.md):
`kind:knowledge`, protocol = OKF over a filesystem / git repo / tarball.

# Two reframings

- **It speaks standard OKF outward, Void Core inward**, bridged by the
  [glossary](/references/voidcore-glossary.md). Consumers who never heard of Void
  Core can use it.
- **Consuming beats producing.** Void Core will be niche, so most bundles in the
  world describe non-Void-Core things. The engine's first job is ingesting/
  validating/serving *any* conformant bundle; producing one from a
  [mantle](/concepts/mantle.md) is the same holiday in reverse.

# Three jobs

1. **Consume** — bundle → queryable concepts (stored internally as runes).
2. **Produce** — mantle → conformant bundle (filtered by tag, e.g. the library OKF).
3. **Validate / refresh** — stale/dead `resource:`→code detection + the `confidence:`
   stamp. This is what makes it an *engine*, not a one-shot exporter.

The engine exposes the bundle as **data** (`ls`/`get`/`query`/`analyze`); a bundle is
*studied* in **FaultSack** (the dedicated OKF study tool), so the engine ships no
visualizer of its own. This bundle you are reading is the engine's hand-authored
**conformance fixture**.

# Status

`current` — **all three jobs have a working v0.1** at `holidays/okf/`
(Python): **consume** (`bundle.py` parser + model + SPEC §5 tag-filter, plus
`voidcore_bridge.py` mapping concepts into real [runes](/concepts/rune.md) through the
C core), **produce** (`voidcore_bridge.py` — mantle → bundle with the `--where`
library filter; round-trips losslessly: 19 concepts + 92 [links](/concepts/links.md)
identical), and **validate** (`validate.py` — conformance + resource-freshness drift +
honesty-convention lint). CLI: `python holidays/okf ls|get|query|validate|produce|analyze`,
plus `manifest.py` (the [app-manifest](/concepts/app-manifest.md) reader). Still `planned`:
exposing these (and [graph analytics](/concepts/graph-analytics.md)) as
[dispatcher](/concepts/dispatcher.md) verbs in the core itself (today they are a
host-side holiday — see [roadmap](/roadmap.md)). Design: [OKF as a core feature](/design/okf-design.md).
