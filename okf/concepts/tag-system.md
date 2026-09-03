---
type: Concept
title: Tag system
description: Void Core's addressing-by-meaning layer — namespaced tags, fundamental axes, a filter grammar, and a weighted tag graph.
resource: core/src/tags/tag.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-07-03T00:00:00Z
---

Because `content` is opaque, **tags are how the core addresses runes by meaning.** A
[rune](/concepts/rune.md) is matched by any of its tags, its own `name`, or
`glyph:<name>`.

# Pieces

- **Namespaced tags** — `group:science`, `status:draft`, `month:june`.
- **Fundamental axes** — every namespace classifies into one of `where`, `what`,
  `who`, `when`, `state`, `free`. This is the interlingua that lets two tag sets
  merge by typed union.
- **Filter grammar** — `AND`/`OR`/`NOT` (+ `&&`/`||`/`!`), parentheses, implicit-AND.
  Used by `ls --tag`, `@<expr>` group-targeting, and `foreach`.
- **Weighted tag graph** — `relate`/`related`/`unrelate`: tags relate to each other
  with weights, and a weight here means **similarity**. Note that this is now the
  *second* weighted graph in the model: since 0.2.14 a [link](/concepts/links.md)
  weight may be an attribute's **value**. Different vertices (tags vs runes),
  different meaning, and nothing joins them — worth knowing before conflating them,
  and before reading weights numerically across a mantle
  ([graph analytics](/concepts/graph-analytics.md)).
- **One evaluator over the FFI** — `vc_tag_match(expr, tags_json)` (Python:
  `VoidCore.tag_match(expr, tags)`) evaluates the grammar against a bare bag of
  tags, stateless and thread-safe. Hosts filtering *external* entities — e.g.
  [holiday](/concepts/holiday.md) rows behind `effect query "<expr>"` — evaluate
  through this seam instead of keeping per-host grammar copies that drift, so
  query-over-holiday means exactly what `ls --tag` means. SPEC §5.

The OKF honesty vocabulary (`status:` / `audience:` / `confidence:`) is just tags on
these axes — `status:` lands on the `state` axis.

**`kind:` is an ordinary app namespace, not a reserved one.** It classifies to the
`what` axis and applications use it freely. That is why a [rune](/concepts/rune.md)'s
*kind* (`entity`/`act`/`measure`) is queried with `ls --kind` rather than exposed as a
`kind:<k>` tag the way `glyph:<name>` is: reserving it would have silently changed
what every existing `kind:vegetable` tag matches.

# Status

`current`. Membership, axes, the full filter grammar, the weighted tag graph, and
the `vc_tag_match` FFI export are implemented in the [C core](/components/c-core.md).
See `SPEC.md` §5.
