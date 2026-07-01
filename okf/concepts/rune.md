---
type: Concept
title: Rune
description: The atomic editable unit of Void Core; identity + glyph + facets + tags + opaque content.
resource: core/src/model/rune.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-06-18T00:00:00Z
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
| `relations` | reserved — to become first-class [links](/concepts/links.md) |

# Rune as a monoid

A rune is conceived as a **monoid**: runes compose, with an identity element, in a
way the [interaction-net](/concepts/interaction-nets.md) foundation makes precise.
Runes live inside a [mantle](/concepts/mantle.md) and reference each other by name
(a [link](/concepts/links.md)).

# Status

`current`. Implemented in the [C core](/components/c-core.md); see `SPEC.md` §3.2. The
`relations` field is persisted but unused, pending the [links](/concepts/links.md)
unification.
