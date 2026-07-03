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
  with weights. A precursor to first-class [links](/concepts/links.md).
- **One evaluator over the FFI** — `vc_tag_match(expr, tags_json)` (Python:
  `VoidCore.tag_match(expr, tags)`) evaluates the grammar against a bare bag of
  tags, stateless and thread-safe. Hosts filtering *external* entities — e.g.
  [holiday](/concepts/holiday.md) rows behind `effect query "<expr>"` — evaluate
  through this seam instead of keeping per-host grammar copies that drift, so
  query-over-holiday means exactly what `ls --tag` means. SPEC §5.

The OKF honesty vocabulary (`status:` / `audience:` / `confidence:`) is just tags on
these axes — `status:` lands on the `state` axis.

# Status

`current`. Membership, axes, the full filter grammar, the weighted tag graph, and
the `vc_tag_match` FFI export are implemented in the [C core](/components/c-core.md).
See `SPEC.md` §5.
