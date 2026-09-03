---
type: Concept
title: Quantity
description: What a number IS — measurement levels, units, and the points-versus-vectors distinction that decides whether a value belongs in a field or on an edge.
resource: core/src/glyph/glyph.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-09-03T00:00:00Z
---

A number on its own is not a value. `5` is not a speed, `12` is not a column, and
`2026-09-03` is not a duration. What makes a number a value is **which operations
are legal on it** and **what unit it is in** — and Void Core now lets a
[glyph](/concepts/glyph.md) say both, in one shape, wherever a quantity appears:

```json
{ "level": "ratio", "unit": "grid-columns", "min": 1, "max": 12 }
```

The same object annotates a glyph **field** (`kinds: {"<field>": …}`) and a
**measure rune** (`quantity`, written by the `measure` verb). That is deliberate: an
application that keeps a value in a field and one that puts it on an
[edge](/concepts/links.md) are describing the same quantity, and must not have to say
it two ways.

Everything here is **declarative**. The core stores the annotation, refuses a
nonsense one, and never coerces, clamps or converts a value. This page exists so the
reasoning is in front of whoever is choosing.

# "Number" is four types

Distinguished by which operations mean anything:

| level | legal | example |
|---|---|---|
| `nominal` | `=` | a tag; a category |
| `ordinal` | `<` | `compact` < `title` < `full` |
| `interval` | differences; **no true zero** | a date, a time, a temperature in °C |
| `ratio` | ratios; a true zero | a weight, a count, a speed, money |

The mistakes this prevents are ordinary ones: averaging an ordinal, summing a
temperature, computing a ratio of two dates.

# Points and vectors — the distinction that does the work

A **vector space** has addition and scaling. An **affine space over it** is a set of
**points** where you may subtract two points to get a vector, and add a vector to a
point — but you may **not** add two points, and you may **not** scale a point.

- *"Half of September 3rd"* — meaningless. That is scaling a point.
- *"A quarter past twelve"* — fine. Point + vector.
- *"How long until the posada"* — fine. Point − point.

**Dates and coordinates are points. Speed, health and money are vectors.** This is
standard mathematics and most readers already know it. That is not an argument
against writing it down; it is the argument *for*. A reader who knows it will not
necessarily apply it at the moment they are choosing between a field and an edge.

# The consequence: field or edge?

An [edge](/concepts/links.md) weight is a **magnitude**. So:

> **Values on edges is correct for ratio-scale vector quantities, and wrong for
> points.**

A date has no magnitude; it has a *position*. A coordinate is the same, and already
has its own home — `placement`, the view slice, which is where positions go and not
into `content`.

The other half of the choice is about who reads the number:

> **If a number is read by RULES that produce new structure, it belongs on an edge
> where the rules can see it. If it is read only by RENDERERS, it belongs in a
> field.**

In a game, health and speed and strength *are* the mechanics — they change every
frame and feed rules that produce new structure. Those belong on edges. In an
outreach app, a date, a grid column and a coordinate are fixed attributes of a record
read by renderers and by nothing else; turning `contact.email` into an edge would
make the commonest operation in that application worse for no gain.

Neither answer is second-class. Fields are not deprecated by any of this and never
will be.

# A vector quantity is one thing, not three edges

A position is three components that transform as a unit. Splitting it into three
edges to `x`/`y`/`z` loses that they are one vector — but `weight` is not becoming an
array either, because [links](/concepts/links.md) already answer this: **an edge
carries `relation`, `direction` and `weight`, and anything else must be reified as a
rune.** A multi-component quantity is a noun the model was missing. And a position in
particular was never a candidate anyway, being a point.

# Two weighted graphs, which is worth stating

Since 0.2.14 the state document holds two weighted graphs that mean different things:

| graph | vertices | a weight means |
|---|---|---|
| `layout.edges` ([links](/concepts/links.md)) | runes | a strength, **or** an attribute's value (§3.7.1) |
| `mantle.tags[…].near` ([tag system](/concepts/tag-system.md)) | tags | similarity |

Nothing joins them, and neither reads the other. Anything that reads weights
numerically across a mantle — [centrality, clustering](/concepts/graph-analytics.md) —
must not assume they are commensurable.

# Status

`current` (2026-09-03). The annotation shape, its validation, the `measure` verb and
the `values` reading are in the [C core](/components/c-core.md); `SPEC.md` §3.3.2 and
§3.7.1, conformance `19-kinds-and-values.vs`. The core does not interpret levels or
units beyond refusing an unknown one — unit *algebra* (checking that `m/s` times `s`
is `m`) is not built and is not scheduled. Worked out by
[Void Hormiga](/concepts/holiday.md) against real dates and coordinates and offered
to this bundle, 2026-09-03.
