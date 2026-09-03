---
type: Foundation
title: Interaction nets
description: The mathematical foundation for runes, mantles, and rewriting — the formalism is decided and its executor (Reduce) is built.
resource: okf/design/interaction-nets-theory.md
tags: [status:current, audience:dev, confidence:asserted, foundation, research]
timestamp: 2026-08-09T00:00:00Z
---

**Interaction nets** ([Lafont](/sources/lafont-interaction-nets.md)) are the mathematical
foundation Void Core is built
toward: [runes](/concepts/rune.md) as agents/monoids, a [mantle](/concepts/mantle.md)
as a net whose `rules` are rewrite rules, and edges between them as
[links](/concepts/links.md). They make "a mantle controls the rewrite rules of its
runes" precise, with the discipline that effects form a commutative monoid (no
vicious cycles).

# The deliberate split

- **Modeled**: rules and the weighted tag graph are *stored and inspected* in the core.
- **Reduced**: the **rule reducer** that executes rewrites was deferred on purpose —
  "model it as a net now, reduce it later" — and has since been built as
  [Reduce](/concepts/reduce.md) (2026-06-28).

Not in scope: building an interaction-net *bytecode VM* or compiling anything to nets.
Void Core is an overlay that *expresses* the model, not a reduction runtime.

# Two things about the Greek letters

**γ, δ and ε are Lafont's, and stay Lafont's.** When the three
[rune kinds](/concepts/rune.md) were proposed they were proposed as *gamma / delta /
epsilon* runes, after the interaction combinators. They shipped as
`entity`/`act`/`measure` instead, and the reason is concrete rather than aesthetic:
Void Maiz already uses those letters in Lafont's original sense **and about glyphs** —
its reducer contract documents `swap` as "Lafont's γγ" against "δδ's crossing look",
and maps glyph → aux-port count. Worse, **ε is the eraser**: an arity-*zero* agent
whose whole job is to terminate a wire, which is close to the opposite of "a concept
that carries a value" — the one of the three that most needs to be understood. Two
sibling projects using γ/δ/ε for different things, both about glyphs, in one stack, is
a confusion that would have been created on purpose. So this page does not have to
reconcile anything, which was the point.

**An `act` rune is an agent.** The reification that makes *"Superman flies across the
sky"* expressible — the verb as a node with typed ports for its roles, participants
wired to those ports — is exactly an interaction-net agent, and exactly what
`layout.edges` already stores with its `i:j` port pairs. Rune kinds did not add a
mechanism here; they named the one that was already underneath.

# Status

`current` — the **formalism is decided** (the chosen foundation) and its **executor is**
[Reduce](/concepts/reduce.md), **built (2026-06-28)** including the `reduce`
[dispatcher](/concepts/dispatcher.md) verb. The interaction-net core (agents with
principal + auxiliary ports, active pairs, rules, reduction to normal form, strong
confluence on the restricted glyph-pair form) is realized in `VoidCore/reduce/`; the §4
glyph **port signatures** are the `Agent.arity` / port model there. What remains is the
general-rule extension (sub-pattern / tag-expression LHS without the confluence
guarantee — see [roadmap](/roadmap.md)), not new foundations. The full γδε
interaction-combinator system reduces confluently under the executor's test suite. See
[interaction nets — theory](/design/interaction-nets-theory.md), [transform layers](/design/transform-layers.md), [Reduce](/concepts/reduce.md).
