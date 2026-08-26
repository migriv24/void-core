---
type: Engine
title: OKF engine
description: The Open Knowledge Format as a Void Core holiday — consume, produce, and validate knowledge bundles.
resource: holidays/okf/__main__.py
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-08-09T00:00:00Z
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

# Generated bundles: the two halves of a concept

A bundle can also be *produced from a running program* — a harvester emits
[dispatcher](/concepts/dispatcher.md) commands that build [runes](/concepts/rune.md),
and `produce` writes the markdown (Void Maiz's surface census is the first such client;
see [UI / UX](/concepts/ui-ux.md)). That raises the machine-half/human-half problem: a
re-harvest must not eat commentary a person wrote about a generated page.

The engine answers it **without any text merge**, because the truth is a rune and not a
file: the harvest lives in `content.body`, human prose in `content.notes` — *different
fields of the same rune* — and `produce` writes body, then notes, separated by an
`<!-- okf:notes -->` marker. A re-harvest overwrites `body` and structurally **cannot**
touch `notes`. `consume` splits the marker back into the two fields, so the round-trip
stays lossless; a file without the marker is all body (permissive consumption), and a
rune without notes produces byte-identical output to before. Notes are authored through
the dispatcher (`set <concept> notes "…"`) and their markdown links join the graph like
any other.

# Status

`current` — **all three jobs have a working v0.1** at `holidays/okf/`
(Python): **consume** (`bundle.py` parser + model + SPEC §5 tag-filter, plus
`voidcore_bridge.py` mapping concepts into real [runes](/concepts/rune.md) through the
C core), **produce** (`voidcore_bridge.py` — mantle → bundle with the `--where`
library filter, plus the `body`/`notes` split above; round-trips losslessly: 50 concepts
+ 254 [links](/concepts/links.md) identical), and **validate** (`validate.py` — conformance + resource-freshness drift +
honesty-convention lint). CLI: `python holidays/okf ls|get|query|validate|produce|analyze`
— a read surface governed by the same altitude rule as the dispatcher
([context optimization](/design/context-optimization.md)), so **`get --head`** returns the
header and link graph without the body (the triage read), and all output is UTF-8 with
`errors="replace"` so a concept using mathematical notation is readable on any console —
plus `manifest.py` (the [app-manifest](/concepts/app-manifest.md) reader). Still `planned`:
exposing these (and [graph analytics](/concepts/graph-analytics.md)) as
[dispatcher](/concepts/dispatcher.md) verbs in the core itself (today they are a
host-side holiday — see [roadmap](/roadmap.md)). Design: [OKF as a core feature](/design/okf-design.md).
