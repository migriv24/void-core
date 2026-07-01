---
type: Concept
title: Scry
description: Pure projection/resolution — views over state (+ optional holiday snapshot) + context; tag-eval, the round-trip law, and materialize live here.
tags: [status:planned, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-06-28T00:00:00Z
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
- **`materialize`** — the one explicit, undoable action that freezes a scryed projection
  back into owned state (archival "baking"); never silent. Distinct from Reduce's
  transient reference-expansion. `materialize(..., stamp=<field>)` records **provenance** —
  `provenance(data)` is a stable, order-independent snapshot id — so an archive carries
  proof of *what snapshot it captured* (and a reader can tell if the live data still matches).

Holiday-backed data is resolved from a *snapshot*, never folded into authoritative state
at edit time (bake-into-state is a bug magnet; derive-from-snapshot is the default).

# Status

`planned` as a full layer, but **most of it is now built (2026-06-28)**:

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

Still `planned`: context-parameterized `resolve` that pulls from a live
[holiday](/concepts/holiday.md) **snapshot** (the `scry(state, snapshot, context)` shape),
and exposing `scry`/`materialize` as dispatcher verbs. The motivating cases are record⇄rune
mappings (`ls --tag`), live galleries, bilingual variants, and reproducible archives; the
[OKF engine](/components/okf-engine.md) is already a scry (mantle → concept bundle). Design:
[transform layers](/design/transform-layers.md).
