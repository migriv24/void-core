---
type: Concept
title: Scry
description: Pure projection/resolution — views over state (+ optional holiday snapshot) + context; tag-eval, the round-trip law, and materialize live here.
resource: scry/projection.py
tags: [status:current, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-08-17T00:00:00Z
---

**Scry** is the read-side verb (sibling of [Reduce](/concepts/reduce.md) and
[Temper](/concepts/temper.md)): derive **views** from owned state, an optional
[holiday](/concepts/holiday.md) **snapshot**, and a **context** (`{locale, audience,
date, role}`) → one [mantle](/concepts/mantle.md), many views. Pure: same inputs ⇒
byte-identical output (so live-preview == final render == archived send).

- The [tag-expression evaluator](/concepts/tag-system.md) is the shared primitive here.
- **Round-trip law** for any scry backing persistence: `unscry(scry(x)) == x`
  (property-tested) — makes silent-data-loss projections structurally impossible.
- A **`Lens`** (`scry/lens.py`) is the *bidirectional* case: a `forward` projection + its
  `backward` inverse, bundled with the round-trip law (`lens.check(samples)`). A persistence
  mapping wants this — e.g. a holiday's record⇄rune mapping (`LocalJsonHoliday.lens()`),
  used for read **and** write **and** persistence so the three can't drift. The failure it
  prevents: a persistence mapping written separately for read, write, and disk drifts into a
  lossy-tag bug between the copies; one `Lens` collapses them to a single tested mapping.
  `compose`/`identity`/`pipeline` chain lenses through a **pivot** — never write a direct
  A→B adapter when A→pivot→B exists, since adapters cost one per *pair* and a pivot costs
  one per *format*, and the composite inherits the law from its legs.
- **`materialize`** — the one explicit, undoable action that freezes a scryed projection
  back into owned state (archival "baking"); never silent. Distinct from Reduce's
  transient reference-expansion. `materialize(..., stamp=<field>)` records **provenance** —
  `provenance(data)` is a stable, order-independent snapshot id — so an archive carries
  proof of *what snapshot it captured* (and a reader can tell if the live data still matches).

Holiday-backed data is resolved from a *snapshot*, never folded into authoritative state
at edit time (bake-into-state is a bug magnet; derive-from-snapshot is the default).

# Status

`current` — **built (2026-06-28)**, including the `scry`/`materialize`
[dispatcher](/concepts/dispatcher.md) verbs and Selectors authored as data
(`voidcore.spec.selector_from_spec`, `config.transform.selectors`):

- **Round-trip law** — `VoidCore/scry/roundtrip.py`, `from voidcore import
  check_roundtrip`. Verified LOSSLESS on real record⇄rune mappings and shown to catch the
  silent-data-loss class it targets.
- **Projection + selectors + context + `materialize`** — `VoidCore/scry/projection.py`
  (`from voidcore import scry, materialize, tag_match, Context, Selector`). `scry(runes,
  where=, select=, sort=, limit=, context=)` projects views; `Selector` is a projection
  *as data*; `Context` (`{locale, audience, date, role}`) is carried explicitly so output
  is reproducible; `materialize` is the one explicit, non-mutating bake of a resolved
  projection into owned state.
- **One shared tag evaluator** — `tag_match` is a pure-Python twin of the C core's
  `vc_filter_eval`, **conformance-tested 13/13 against `ls --tag`**
  (`scry/conformance_test.py`); projection/purity covered by `scry/projection_test.py`.

# `context.date` is a query, not a time model

`Context` carries a `date`, and this page reads as though that is *the* time axis. It is
not — it is the **query side of one axis**, and a host modelling anything historical needs
to know which one. Void Reyna's civic record (2026-08-17) found three:

| axis | what it is | where it lives |
|---|---|---|
| **transaction** | when *we* learned it | the command log + undo history — **already yours**, and never on a rune |
| **valid** | when it was true in the world | the application's data (`from`/`until` fields) |
| **decision** | when the body actually *voted* | the application's data (an `adopted` field) |

What Scry contributes is the reframe that made the rest tractable: *"what did this policy
say in 2019"* is a **projection**, not a versioning mechanism — same state, different
context, and a pure function between them. Reyna deleted a versioning design on the
strength of that.

But `date`-in-context is what you **filter with**; the data still has to carry its own two
dates. Collapsing valid and decision time into one field is invisible while adoption is
prospective and immediate, and breaks on a **retroactive** amendment (adopted in March,
effective back to January): two assertions then carry equal strength and merge to a
conflict the record does not actually have. Nothing here needs an engine feature — the
point is only that the core owns transaction time and the host owns the other two.

**Portable contract** — `conformance/scry/` (8 pure-JSON cases + a ~110-line runner) for
hosts implementing Scry outside the Python seam. It pins the operation order
(filter → sort → limit, the classic port bug), the two *reserved* tag matches a port
that only searches `tags` will miss (name-as-tag, `glyph:<glyph>`), `materialize`'s
merge/append/stamp behavior, and `provenance` — Void Core's one existing **byte-level**
commitment (canonical JSON + SHA-256, first 16 hex), including the integer-vs-float
hazard that makes `1` and `1.0` hash differently across languages.

Still `planned`: context-parameterized `resolve` that pulls from a live
[holiday](/concepts/holiday.md) **snapshot** (the `scry(state, snapshot, context)` shape —
see [roadmap](/roadmap.md)). The motivating cases are record⇄rune
mappings (`ls --tag`), live galleries, bilingual variants, and reproducible archives; the
[OKF engine](/components/okf-engine.md) is already a scry (mantle → concept bundle). Design:
[transform layers](/design/transform-layers.md).
