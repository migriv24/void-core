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
annihilate, commute, expand, to_net, from_net`). A faithful
[Lafont](/sources/lafont-interaction-nets.md) interaction-net
reducer: a `Net` of `Agent`s with a principal + auxiliary ports and a symmetric wiring map
(`reduce/net.py`, the deferred §4 port-signature groundwork), and the `Reducer`
(`reduce/reduce.py`) — glyph-pair rule registration with the conflict guard, `reduce()` to
normal form (canonical + pluggable scheduling, opaque agents, a `max_steps` termination
guard), and `annihilate`/`commute`/`expand`/general `rule` constructors. `to_net`/`from_net`
bridge a [mantle](/concepts/mantle.md) (port indices ride the edge `relation` as `"i:j"`).
Agents carry the rune's `tags` through the round-trip (fixed 2026-07-07 on a VLS report —
they were dropped); agents *created by rules* during reduction start tagless.

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

The executor's semantics are also a **portable contract** (2026-07-09):
`conformance/reduce/` states them language-neutrally and carries pure-JSON conformance
cases + a runner, so a host in another language can implement Reduce and prove it matches
the reference. The Python implementation stays the oracle; a future C-ABI reduce would be
tested by the same cases.

**Contract evolution (2026-07-14, from Void Maiz's first daily-use findings):**
(1) annihilation has **two flavors** — index-straight `A(i)↔B(i)` (δδ, the crossing
picture) and index-swapped `A(i)↔B(n+1−i)` via `"swap": true` (γγ, the mirrored
parallel-arcs picture); the asymmetry is load-bearing (it's what makes the combinators
universal), and with one flavor γγ and δδ are indistinguishable. (2) **internal redex
wires resolve by default** (full Lafont): the executor chases the wire equations through
the redex via union-find — two external ends bridge, one stays free, a closed loop
**vanishes** (ruling: the net model can't represent an agentless wire; hosts wanting
loops-as-values count them outside the contract); under commute the equations join the
corresponding copies' *principals* — a fresh active pair, Lafont's own picture.
`strict_locality=True` (case key `"strict_locality"`) restores the restricted subset's
`locality` rejection. Cases 11–14 pin swap, the internal bridge, the loop vanish, and
the internal-wire commute; case 09 re-pins strict mode.

**Reduction-created agents are named from the redex** (0.2.6, Void Palabra's ask):
`H(rule glyphs, the two parent ids, an ordinal)` rather than a running counter. Confluence
alone promises the same normal form only *up to renaming*, which is the same shape but not
the same **bytes** — so two peers picking different, equally valid, redex orders used to
produce structurally identical nets whose agents had different `spirit.name`s. That blocks
merge-by-reduction (two peers reaching the same normal form is a theorem on this subset, so
merge should be free) and it is a real divergence, since a name is what
[links](/concepts/links.md) reference and tag expressions match. Now normative in the
contract, pinned by case 15 (`pin_ids` + 16 randomized schedules must agree on the literal
ids) and by a law test; an implementation minting ids sequentially passes every other case
and fails that one. **15/15 conformance.**

Still `planned`: general (sub-pattern / tag-expression) rule LHS without the confluence
guarantee, and data-form `expand` (it needs a custom build fn, so it stays code-registered).
Foundation: [interaction nets](/concepts/interaction-nets.md).
