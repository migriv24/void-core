---
type: Source
title: Lafont — Interaction Nets / Interaction Combinators
description: Yves Lafont's interaction nets and the three universal interaction combinators — the external work the whole Reduce layer and its conformance contract rest on.
resource: https://doi.org/10.1006/inco.1997.2643
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-08-09T00:00:00Z
---

# What it is

Two works by **Yves Lafont**, recalled as:

- **"Interaction Nets"**, POPL 1990 — the model itself: agents with one *principal*
  port and *n* auxiliary ports, wires connecting ports one-to-one, and rewriting that
  fires only on an **active pair** (two principal ports wired together). Rewriting is
  local and strongly confluent when at most one rule exists per unordered symbol pair.
- **"Interaction Combinators"**, *Information and Computation* 137(1), 1997 — the
  universality result: three symbols (γ constructor, δ duplicator, ε eraser) with
  annihilation and commutation rules suffice to encode any interaction net system.

# What Void Core uses it for

This is the most load-bearing external claim in the bundle. It is the foundation of
[interaction nets](/concepts/interaction-nets.md) and of [Reduce](/concepts/reduce.md),
and — unusually — it has been promoted into a **normative contract** that other projects
implement against (`conformance/reduce/`). Specifically we assert, on this authority:

- principal-port-only rewriting, and "active pairs only reduce" (so feedback cycles are
  inert rather than divergent);
- **strong confluence by construction** on the restricted subset (≤1 rule per unordered
  glyph pair) — this is why the [Void Palabra](/design/index.md) merge-by-reduction plan
  is a theorem rather than a hope;
- annihilation's **two flavors**, and that the asymmetry is *load-bearing*: γγ links
  index-reversed (`A(i) ↔ B(n+1−i)`, the mirrored parallel-arcs picture) while δδ links
  index-straight (`A(i) ↔ B(i)`, the crossing picture) — with the claim that if both used
  one flavor, γ and δ could not be distinguished and the universality argument breaks;
- commutation's m×n copy grid, and that an internal aux-to-aux wire resolves by chasing
  the wire equations through the redex (two external ends bridge, one stays free, a closed
  loop vanishes) — "Lafont's own picture";
- that interaction combinators are **Turing-complete**, which is why the executor must
  carry a `max_steps` guard rather than promise termination.

# Why it is credible

A peer-reviewed journal result by the author of the model, and the standard citation for
interaction-net universality. The risk here is not the source's authority; it is whether
*our restatement* of it is faithful.

# What a verification pass should check

This page is `confidence:asserted` — written from recall, **not** checked against the
artifacts. In descending order of how much would break:

1. **The γγ-swapped / δδ-straight asymmetry.** This is the sharpest and most falsifiable
   claim we make, it is pinned in `conformance/reduce/` cases 03 and 11, and it did **not**
   come from the paper — it arrived as a Void Maiz field report (2026-07-13) from a running
   InteractionCombinators demo and was adopted into the contract on the strength of the
   argument, not the citation. Check which of γ/δ is the swapped one, and whether
   "universality breaks without the asymmetry" is Lafont's argument or our paraphrase.
2. **The symbol naming.** Confirm γ = constructor, δ = duplicator, ε = eraser, and that
   the ε rules are erasure rather than commutation.
3. **Confluence's exact statement.** We claim *strong* confluence (all reduction paths have
   the same length and the same normal form), not merely confluence. Confirm which Lafont
   proves and under what restriction.
4. **The loop-vanish ruling.** We ruled that a closed wire loop with no agents on it simply
   disappears, because the net model cannot represent an agentless wire. Check whether
   Lafont's formulation admits such a loop as an object; if it does, our contract deviates
   and should say so rather than imply agreement.
5. **The citations themselves** — year, venue, volume, and the DOI in `resource:`.

Items 1 and 4 are the ones where being wrong would mean other projects' implementations
are conforming to a mistake.
