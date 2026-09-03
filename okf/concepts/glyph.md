---
type: Concept
title: Glyph
description: A rune's declared type — its content fields, its kind, and (separately) how it may be presented in each modality.
resource: core/src/glyph/glyph.c
tags: [status:current, audience:library, audience:dev, confidence:verified, foundation]
timestamp: 2026-09-03T00:00:00Z
---

A **glyph** declares what a [rune](/concepts/rune.md) **is**: which content fields it
has, what [kind](/concepts/rune.md) of thing it is (`entity` / `act` / `measure`),
and how its numbers should be read ([quantity](/concepts/quantity.md) annotations).
Separately, and optionally, it declares how the rune may be **presented** — one
answer per modality. The core ships a small registry; applications add their own.

# Two questions, and why they were one object

Until 2026-09-03 a descriptor answered two questions at once.

**"What is this rune?"** — the schema. Which fields exist, what shape `content` has.
This is a fact about the rune: true in every mantle, in every context, for every
output, forever.

**"How does this rune appear?"** — the presentation. A color, a face, ports, an
editor, a label — and, in a host with two renderers, a chain of `else if (glyph ==
"audio")` cases in each of them. This is a fact about a rune **in a modality**, and
there is one per modality.

The conflation was natural: for every application built so far there is exactly one
presentation per type per surface, so schema and presentation are in bijection and
the distinction never pays rent. A descriptor field that *is* the image asset
genuinely feels like a property of the thing it depicts.

It stops working the moment a rune has more than one representation — a sprite, a
sound, an animation, a card, a table row, a paragraph in an email. Presentation then
stops being a property of the rune and becomes a **function from the rune to a
modality**. And once it is a function it can be *derived* rather than authored, which
is the whole point of the architecture this is a step toward: a rune is the
representation-independent thing, and the glyph is what some renderer — eventually a
model — makes of it. For a diffusion model, the glyph of the rune "superman" is the
pixels.

# The shape

| key | half | meaning |
|---|---|---|
| `glyph` | — | the name; what `rune new <glyph> <name>` takes |
| `label`, `editor` | schema-ish | human name and which editor drives it (unchanged) |
| `fields` | **schema** | the content field names |
| `kind` | **schema** | `entity` \| `act` \| `measure`; default `entity` |
| `kinds` | **schema** | per-field [quantity](/concepts/quantity.md) annotations |
| `presentations` | **presentation** | `{ "<modality>": { ... } }`, stored and never interpreted |

`presentations` is additive. Whatever `hints` a host already puts on a descriptor
keeps working, and `canvas` is the conventional home for them.

**The naming is deliberately unresolved.** Under the author's original intent the
*rendering* is what should be called the glyph and the schema should be called
something else — which inverts what `rune new <glyph>` and every host already mean.
Renaming across the ABI breaks every host at once, so: split the concept now under a
new name, decide the naming later, deprecate nothing in a hurry. What matters is that
the two stopped sharing one object; what they are called is recoverable. Recorded as
an open question in `SPEC.md` §12.

# Declared vs registered — which half travels

A descriptor reaches a manager two ways, and only one of them survives the trip:

| | lives in | exported? | written by |
|---|---|---|---|
| **registered** | the manager | no — host config | `vc_register_glyph` at boot |
| **declared** | `state.glyphs` | **yes** | `glyph declare '<json>'` |

Until 0.2.14 only the first existed, and so **a bundle carried its runes but not
their meaning.** Open it on a machine whose host did not register the same
descriptors and the content survives verbatim in the document while the projection
has no fields — the data is *present and unreachable*. That is why an organization's
own record type could not survive being handed to somebody, and it is the reason
declarations exist.

A declaration **shadows** a registration of the same name: it is the one that
traveled with the data, so it is the one that describes it. `glyphs` reports each
descriptor's `source`, so the shadowing is never silent. Undeclaring is refused while
runes still carry the glyph, and makes a shadowed registration visible again rather
than deleting it.

Declaring is an ordinary command — logged, journaled, undoable, mergeable — which is
what makes it trustworthy. It undoes like `rune new` rather than sitting outside
history like `config`, because **a schema is authored content**: it travels with the
document and is what makes the runes readable.

# The descriptor is a host contract

`glyphs` returns the descriptor as `data`, with `fields`, `kind` and `source`
resolved so one shape answers whatever the author wrote. A host may read `fields`,
`kind`, `kinds` and `presentations` from it and **should not keep a second copy of
its own schema**. Void Hormiga kept one whose comment read *"the one place the schema
is written twice, until a core verb exposes glyph descriptors to hosts"*; this is
that verb, said out loud rather than left implicit.

A malformed descriptor is **refused, not half-stored** — an unknown `kind`, an
unknown measurement `level`, a `kinds` key naming a field the glyph does not declare.
A descriptor outlives the session that wrote it and travels to hosts that never saw
the code, so a typo in one is a fact about a type nobody downstream can question.

# Built-in glyphs

`text`, `richtext`, `image`, `imageList`, `color`, `link`, `group` — all `entity`.
Applications add domain glyphs (a `dialogueLine` for a comic, a `cutsceneAction` for
a game, a `contact` for an outreach app).

# Relation to OKF type

OKF's required `type` field maps to a glyph — but consumed OKF concepts use **one
generic `okf-concept` glyph** with the OKF type carried as a `type:<value>`
[tag](/concepts/tag-system.md), because OKF types are open-world and glyphs are
registered. See the [glossary](/references/voidcore-glossary.md).

# Status

`current` (verified 2026-09-03, conformance `18-glyph-declarations.vs` +
`bindings/python/glyph_declaration_test.py`). Declarations, kinds, quantity
annotations, `presentations` storage, the `glyph` verb and the resolved-descriptor
contract are all in the [C core](/components/c-core.md); see `SPEC.md` §3.3.
`planned`: host-language `render`/`describe` *callbacks* over FFI, and the derived
(model-backed) presentation this split is a step toward — explicitly out of scope
today. Asked for by [Void Hormiga](/concepts/holiday.md) 2026-09-03, relaying the
author's design intent.
