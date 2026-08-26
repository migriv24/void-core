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
