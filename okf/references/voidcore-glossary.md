---
type: Dictionary
title: OKF ⇄ Void Core glossary
description: Bidirectional dictionary between standard OKF vocabulary and Void Core's internal terms.
tags: [status:current, audience:dev, confidence:asserted, reference]
timestamp: 2026-07-01T00:00:00Z
---

The [OKF engine](/components/okf-engine.md) speaks **standard OKF** on the outside and
**Void Core** on the inside. This dictionary is that seam. It ships in a bundle only
when the bundle describes a Void Core application; arbitrary external bundles need no
dictionary.

# Mapping

| OKF term (public) | Void Core term (internal) | notes |
|---|---|---|
| Concept | [rune](/concepts/rune.md) | one knowledge / editable unit |
| Concept ID (path minus `.md`) | `spirit.name` (+ stable `spirit.id`) | export derives collision-free paths from names |
| `type` (required) | a `type:<value>` tag on a generic `okf-concept` [glyph](/concepts/glyph.md) | OKF types are open-world, not registered glyphs |
| `title` | `spirit.name` or a facet | |
| `description` / `resource` / `timestamp` | facets `what` / `where` / `when` | `resource` + `when` are the freshness hooks |
| `tags` | [tags](/concepts/tag-system.md) | verbatim; same field, same grammar |
| body (markdown) | `content.body` (opaque to the core) | |
| notes (markdown after `<!-- okf:notes -->`) | `content.notes` (opaque to the core) | the hand-authored half of a *generated* concept; a re-produce overwrites `body` and cannot touch `notes` |
| markdown link | a [link](/concepts/links.md) (today: `layout.edges`) | untyped directed edge; may dangle |
| Knowledge Bundle | [mantle](/concepts/mantle.md) over a [domain](/concepts/domain.md) | the source dir/repo is the domain |
| `index.md` | `describe <mantle>` (generated) | |
| `log.md` | the logging spine + undo history | |
| `viz.html` (a rendered view) | studied in **FaultSack** (external tool) | the core ships no visualizer; the bundle is exposed as data |

# Honesty vocabulary (standard OKF tags)

| tag | values | meaning |
|---|---|---|
| `status:` | `current` / `planned` / `deprecated` | shipped reality vs future plan |
| `audience:` | `dev` / `library` | which projected bundle a concept belongs to |
| `confidence:` | `verified` / `asserted` / `exploratory` / `stale` | `verified`/`asserted`/`exploratory` are authored; `stale` is stamped by `validate` (resource drift) |

On a [`Source`](/sources/index.md) page the same three authored values read as claims about
an **attribution** — and the reading is sharp enough to be worth stating, because it turns
the axis into a work queue:

| value | on a source page |
|---|---|
| `exploratory` | unsure the attribution is right at all |
| `asserted` | written from a model's recall, **not** checked against the artifact |
| `verified` | someone opened the work and confirmed author, year and claim |
