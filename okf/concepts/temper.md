---
type: Concept
title: Temper
description: Pure normalization — clean owned state to a canonical form after an action (invariants, derived-field defaults). Idempotent.
tags: [status:planned, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-06-28T00:00:00Z
---

**Temper** is the normalization verb (sibling of [Reduce](/concepts/reduce.md) and
[Scry](/concepts/scry.md)). Pure rules run — eagerly, cheaply — after an action to keep
**owned** state consistent: derived-field defaults (e.g. "thumb = images[0]"),
de-duplication, [tag](/concepts/tag-system.md) normalization. It centralizes the
invariants apps currently hand-code on every mutation path.

Idempotent: `temper(temper(x)) == temper(x)`. Pure, deterministic, no effects.

# Status

**Built (2026-06-28)**: `VoidCore/temper/temper.py` (`from voidcore import Temper,
dedupe, member_or_default, default_content, default_tag, normalize_tags, single_tag`).
`Temper(rules)` applies an ordered list of pure `rune -> rune` rules; the library covers
the invariants apps otherwise hand-code on every mutation path — `member_or_default("thumb",
"images")` (the thumb = images[0] juggling in add/remove image), `dedupe`,
`default_tag`/`single_tag` (status normalization), `normalize_tags`.
The **idempotence law** `temper(temper(x)) == temper(x)` and purity (no source mutation)
are property-tested in `temper/temper_test.py`.

Surfaced as the `temper` [dispatcher](/concepts/dispatcher.md) verb (the seam), with an
opt-in **temper-on-write** mode (`Dispatcher.temper_on_write()`) that runs the registered
pass automatically after every mutating verb — so invariants hold even for raw dispatcher
edits (a key correctness ask for any app that exposes a raw dispatcher to users). Still
`planned`: folding the rules into mantle **data** (author-editable, not code) and
one-undo-frame-per-pass. Design: [transform layers](/design/transform-layers.md).
