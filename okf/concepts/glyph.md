---
type: Concept
title: Glyph
description: A rune's editability type — binds a rune to how it is edited and how it is described.
resource: core/src/glyph/glyph.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-06-18T00:00:00Z
---

A **glyph** is a [rune](/concepts/rune.md)'s practical type: it declares the rune's
content fields, which editor drives it, and how to summarize it. The core ships a
small registry; applications register their own.

# Built-in glyphs

`text`, `richtext`, `image`, `imageList`, `color`, `link`, `group`. Applications add
domain glyphs (e.g. a `dialogueLine` for a comic, a `cutsceneAction` for a game).

# Relation to OKF type

OKF's required `type` field maps to a glyph — but consumed OKF concepts use **one
generic `okf-concept` glyph** with the OKF type carried as a `type:<value>`
[tag](/concepts/tag-system.md), because OKF types are open-world and glyphs are
registered. See the [glossary](/references/voidcore-glossary.md).

# Status

`current`. Glyphs are data descriptors today; host-language `render`/`describe`
*callbacks* over FFI are `planned`. See `SPEC.md` §3.3.
