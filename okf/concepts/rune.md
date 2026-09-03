---
type: Concept
title: Rune
description: The atomic editable unit of Void Core; identity + glyph (and its kind) + facets + tags + opaque content.
resource: core/src/model/rune.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-09-03T00:00:00Z
---

A **rune** is the smallest editable unit. The core treats its `content` as opaque —
it only ever sees the rune's identity, its [glyph](/concepts/glyph.md), its six
facets, and its [tags](/concepts/tag-system.md).

# Shape

| field | meaning |
|---|---|
| `spirit` | identity: a frozen `id` (minted once, never reused) + an editable, mantle-unique `name` |
| `glyph` | the rune's editability type — see [glyph](/concepts/glyph.md) |
| `facets` | the six who/what/when/where/why/how strings (always present) — uniform context for a human or LLM |
| `tags` | addressing-by-meaning; the `name` itself doubles as a tag |
| `content` | glyph-specific payload, opaque to the core |
| `placement` | the view slice: where it sits in a spatial view; not undoable |
| `quantity` | **measure runes only** — what it measures ([quantity](/concepts/quantity.md)) |
| `relations` | reserved — to become first-class [links](/concepts/links.md) |

# Three kinds of rune

Not every rune is the same *kind* of thing, and the difference is structural rather
than domain-specific. The kind lives on the [glyph](/concepts/glyph.md), defaults to
`entity`, and nothing migrates — every rune that existed before this is an entity and
stays one.

| kind | is | examples |
|---|---|---|
| `entity` | an explicit thing, which has representations | a contact, an event, a player, an enemy |
| `act` | a change; it modifies how entities are represented, or relates them | `flying`, `across`, `attacks` |
| `measure` | a dimension something has an amount of | `x`, `health`, `speed` |

The argument for the split is about **arity**. An edge label can only ever express a
*binary* relation, and *"Superman flies across the sky"* is at least ternary — an
agent, an act, and a path. There is no way to write it as one labelled edge without
losing a participant. The standard move is to **reify** the relation: make the verb a
node with typed ports for its roles and connect the participants to those ports. That
is RDF reification, and neo-Davidsonian event semantics, and — precisely — an
[interaction-net](/concepts/interaction-nets.md) agent, which is the model this stack
already implements and already stores as `layout.edges` with `i:j` port pairs. **An
act rune is not a new mechanism; it is the mechanism we already had, applied to verbs
instead of only to nouns.**

The triad is the entity/relationship/attribute one that data modelling settled on
about fifty years ago, arrived at here from representation-independence instead. A
`measure` rune is what physics calls an *observable*. An independent derivation
landing on a known triad is a good sign rather than a coincidence.

That *"the core treats `content` as opaque"* is exactly what made this cheap: the
store never knew what a `contact` was, so adding kinds cost the store nothing.

Kinds are **not** a reserved `kind:<k>` [tag](/concepts/tag-system.md) — `kind:` is
already an ordinary app namespace on the `what` axis, and reserving it would silently
change what every existing `kind:vegetable` tag matches. `ls --kind <k>` and
`glyphs --kind <k>` are the queries.

# Rune as a monoid

A rune is conceived as a **monoid**: runes compose, with an identity element, in a
way the [interaction-net](/concepts/interaction-nets.md) foundation makes precise.
Runes live inside a [mantle](/concepts/mantle.md) and reference each other by name
(a [link](/concepts/links.md)).

# Status

`current`. Implemented in the [C core](/components/c-core.md); see `SPEC.md` §3.2,
§3.3.1. Rune kinds shipped 0.2.14 (conformance `19-kinds-and-values.vs`), asked for by
[Void Hormiga](/concepts/holiday.md) relaying the author's design intent. The
`relations` field is persisted but unused, pending the [links](/concepts/links.md)
unification.
