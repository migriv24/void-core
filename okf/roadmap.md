---
type: Roadmap
title: Roadmap
description: Forward-looking index of what is still planned and the near-term build order. Dev-only; excluded from the shipped library bundle. Completed work lives in log.md, not here.
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-07-01T00:00:00Z
---

The forward-looking counterpart to the bundle `index.md` (current listing) and
[`log.md`](/log.md) (history): an index of what is **planned** and the order we intend
to build it. This is `audience:dev` — the shipped library OKF (`status:current AND
audience:library`) leaves it out.

**Convention: this file lists only what is *not yet done*.** When something ships it
moves to [`log.md`](/log.md) and out of here; partially-done items stay, with the
remaining slice stated precisely. This index mirrors `okf query "status:planned"`.

# Remaining on built layers

The three [transformation layers](/design/transform-layers.md) are **built** and folded
into the [dispatcher](/concepts/dispatcher.md) as verbs (`scry`/`temper`/`materialize`/
`reduce`), configurable by code or by data (`config.transform`, `voidcore.spec`). What
remains is integration polish, per the concept pages:

1. **[Scry](/concepts/scry.md)** — context-parameterized `resolve` over a *live*
   [holiday](/concepts/holiday.md) **snapshot** (the `scry(state, snapshot, context)`
   shape). The pure projection/law/`materialize` pieces are done.
2. **[Temper](/concepts/temper.md)** — remaining author-facing polish on rules-as-mantle
   **data** (the `voidcore.spec` compiler exists; the ergonomics of authoring/editing them
   in a mantle are the open part).
3. **[Reduce](/concepts/reduce.md)** — general (sub-pattern / tag-expression) rule LHS
   *without* the confluence guarantee, and data-form `expand` (needs a custom build fn, so
   it stays code-registered today).

# Planned concepts

* [Links](/concepts/links.md) - the passive-connection substrate (partly built as
  `layout.edges`; the fuller loose-connection model remains)
* [Interaction nets](/concepts/interaction-nets.md) - the formalism is decided and its
  **executor is [Reduce](/concepts/reduce.md)** (built); the remaining pieces are the
  general-rule extensions above
* [UI / UX](/concepts/ui-ux.md) - abstract today; open question is *how* an app declares
  its UI/UX, plus a first renderer/representation holiday over the
  [app manifest](/concepts/app-manifest.md)
* [Logging & debug](/concepts/logging-debug.md) - richer debug/trace tooling and a
  Voidscript-driven **test engine** (`assert`-over-dispatcher)
* [OKF engine](/components/okf-engine.md) - v0.1 (consume/produce/validate) is built;
  exposing it — and the built [graph-analytics](/concepts/graph-analytics.md) holiday — as
  [dispatcher](/concepts/dispatcher.md) verbs in the core remains (today both are
  host-side holidays)

# The open-application track

Direction set 2026-07-04 in [the open application](/design/open-application-design.md);
**gated on Miguel's answers to its §8 questions**, then built in phases:

1. **Phase A — contracts as pages** (no code): `host bundle` concept + reserved
   `host.md`/`type: Host`; the **application standard** (3-tier instantiation
   checklist); **two seats** (builder vs operator agents); manifest `ui.*` keys +
   the L0–L3 sandbox-surface ladder; engine-reuse standard + the **mantle capsule**
   envelope.
2. **Phase B — probe holiday** (`holidays/host/`): the local/host OKF as regenerable
   probed facts (first consumer: Hormiga's platform checks).
3. **Phase C — sandbox surface L0/L1**: `render`/`snapshot` effect ops + one
   reference surface; consumed by FaultSack's sandbox tab.
4. **Phase D — capsules + registry**: `mantle export`/import with policy; the tagged
   holiday registry (below) as the discovery layer; `validate --app` lint.

# Deferred / research

* **Inter-application communication** — a protocol layer for the multiple apps built on
  Void Core to talk to each other (capability discovery, message/event passing, shared
  vocabulary) *on top of* the [OKF](/components/okf-engine.md), which already gives them a
  shared self-description. Includes the "update other agents" pattern (done by hand in the
  [transform-layers handoff](/design/transform-layers-handoff.md)) as a first-class concept.
  **Shelved on purpose (2026-06-28)** — promising, but the transformation-layer follow-ups
  come first.
* Lightweight **NLP / embedding holiday** — semantic link-suggestion + context-length
  optimization ([context optimization](/design/context-optimization.md)).
* A tagged **holiday registry** — many holidays selected by tag/capability with fallback
  chains (extends [Holiday](/concepts/holiday.md)).
