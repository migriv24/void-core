---
type: Concept
title: Reduce
description: The graph rewriter — fire a mantle's interaction rules / expand references until normal form, producing a derived mantle.
resource: reduce/reduce.py
tags: [status:current, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-07-01T00:00:00Z
---

**Reduce** is the first of Void Core's three transformation verbs
([Temper](/concepts/temper.md) and [Scry](/concepts/scry.md) are the others). It is the
interaction-net executor: fire a [mantle](/concepts/mantle.md)'s rules / expand
[links](/concepts/links.md) and references until **normal form**, producing a *derived*
mantle (the source is untouched). The "structure compiler" — abstract, rule-/tag-/
reference-bearing mantle → flattened mantle a [holiday](/concepts/holiday.md) can consume.

It stays on the right side of the drift test: it normalizes the overlay's **own
structure**, never computes or runs the host app's domain output. Pure, deterministic,
previewable, undoable; effects never fire from inside it.

# Status

`current` — **built (2026-06-28)**: `VoidCore/reduce/` (`from voidcore import Reducer, Net, Agent,
annihilate, commute, expand, to_net, from_net`). A faithful Lafont interaction-net
reducer: a `Net` of `Agent`s with a principal + auxiliary ports and a symmetric wiring map
(`reduce/net.py`, the deferred §4 port-signature groundwork), and the `Reducer`
(`reduce/reduce.py`) — glyph-pair rule registration with the conflict guard, `reduce()` to
normal form (canonical + pluggable scheduling, opaque agents, a `max_steps` termination
guard), and `annihilate`/`commute`/`expand`/general `rule` constructors. `to_net`/`from_net`
bridge a [mantle](/concepts/mantle.md) (port indices ride the edge `relation` as `"i:j"`).

The **five open forks are resolved** ([transform layers](/design/transform-layers.md)): restricted confluent subset
(≤1 rule per glyph pair, enforced at registration); `reduce(net)->net`, pure, no effects;
active-pairs-only so feedback cycles are preserved + explicit opaque marking; `expand` is
transient (vs Scry's durable `materialize`); explicit + previewable scheduling. The laws —
identity, normal form, **strong confluence under randomized schedules** (the full γδε
interaction-combinator system), the termination guard, locality/linearity preservation,
opaque pass-through, and purity — are property-tested in `reduce/reduce_test.py`.

Surfaced as the **`reduce` dispatcher verb** (the seam): `reduce [--into <name>] [--commit]`
builds the active mantle's net (port indices ride each edge's `relation` as `"i:j"`),
rewrites to normal form, returns the derived mantle (preview; source untouched), and
`--commit` installs it live. The reducer + port signatures are **authored as data**
(`voidcore.spec.reducer_from_spec`, `config.transform.reduce`) so a mantle carries its own
rewrite rules. Tested in `voidcore/reduce_verb_test.py`.

Still `planned`: general (sub-pattern / tag-expression) rule LHS without the confluence
guarantee, and data-form `expand` (it needs a custom build fn, so it stays code-registered).
Foundation: [interaction nets](/concepts/interaction-nets.md).
