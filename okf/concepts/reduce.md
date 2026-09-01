---
type: Concept
title: Reduce
description: The graph rewriter — fire a mantle's interaction rules / expand references until normal form, producing a derived mantle.
resource: reduce/reduce.py
tags: [status:current, audience:dev, audience:library, confidence:asserted, foundation]
timestamp: 2026-08-31T00:00:00Z
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
[links](/concepts/links.md) reference and tag expressions match. Pinned by case 15
(`pin_ids` + 16 randomized schedules must agree on the literal ids) and by a law test; an
implementation minting ids sequentially passes every other case and fails that one.

**The digest is normative too, since 0.2.10** — SHA-256 truncated to 6 bytes, `_r` + 12
lowercase hex, over a `\x1f`-joined key whose two components are sorted separately.
Making the *property* normative while leaving the *hash* implementation-defined was
jointly unsatisfiable, and Void Unity's C# executor proved it by holding the property and
still not producing our bytes: two conforming implementations then diverge silently, and
an add-wins join keeps both copies of every rule-created agent. SHA-256 rather than the
BLAKE2b-48 we shipped first for one reason — every standard library has it, so a second
implementer vendors no cryptographic primitive to be conformant. Case 16 pins the minter
alone (key in, id out, no reduction), so the digest can be debugged without the rewriter.
**16/16 conformance.**

**A mantle can be a rune inside another mantle** (0.2.11, `reduce/box.py`). A net with *n*
free ports is an agent of arity *n*, so a **box** — a glyph declared as "a rune of this
glyph *is* that mantle" — makes the adapter splice the sub-mantle's net in at the rune's
ports. A player mantle of body parts and clothing is one rune in the world, and an amulet
in the world reduces straight through the boundary onto the shirt inside it. Encapsulation
and that reach-in are the **same** mechanism: the parent can address only the interface,
and every other inner port is already wired, so linearity — not a scope check — is what
stops an inner rule meeting an outer agent. An interface *orders* the sub-net's free ports
and may never redefine them. See [mantle composition](/design/mantle-composition.md);
contract in `conformance/reduce/README.md` §7, cases 17-20. **20/20 conformance.**

**Content is authorable as data too** (0.2.12, the `patch` rule). `annihilate` and
`commute` are structural and neither writes content, so *"the equipped dye makes the shirt
purple"* used to be a code-registered `expand` — which put a game's **structure** in data
and its **content** behind a recompile, the wrong way round, since the content half is what
a designer changes on a Tuesday afternoon. `patch` names a survivor by glyph, `copy`s
fields off the consumed agent and `set`s literals, and is **content-only**: same id, glyph,
arity, tags and aux wiring. Keeping the id is what preserves a boxed agent's `<rune>/`
provenance through a rewrite. It is deliberately weaker than an arbitrary rewrite so the
one-rule-per-pair confluence guard still means something, it never creates an active pair,
and same-glyph pairs are refused at compile time (on an unordered pair there is no
non-arbitrary survivor). **A rule-created agent inherits its parents' box** — the longest
common box path, or none when they share none, so ambiguity is reported by saying nothing.
**25/25 conformance.**

Still `planned`: general (sub-pattern / tag-expression) rule LHS without the confluence
guarantee; data-form `expand`, for a rule that must *restructure* on contact rather than
patch content; and re-boxing a normal form on the way out — which is
[declined rather than pending](/design/mantle-composition.md), because after a rewrite
spanning a boundary there is often no fact of the matter. Foundation:
[interaction nets](/concepts/interaction-nets.md).
