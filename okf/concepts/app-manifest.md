---
type: Concept
title: App manifest
description: How a Void Core app introduces itself — a reserved OKF concept (app.md, type Manifest) carrying identity + an optional representation layer, read via the OKF holiday.
resource: holidays/okf/manifest.py
tags: [status:current, audience:dev, audience:library, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

Every app builds on the same `core`, so there's a standard way for one to **introduce
itself** — instead of every tool (a launcher, a registry, FaultSack — the external OKF
study tool) re-scraping prose. The manifest is a reserved OKF concept at the bundle root, **`app.md`** of
**`type: Manifest`**, read with `read_manifest()` in the [OKF engine](/components/okf-engine.md)
holiday. It is **data, not engine behavior**: the core defines the shape and renders nothing
(same discipline as [layout/rules](/concepts/mantle.md) — persisted, not executed).

Why the OKF route (over a runtime [glyph](/concepts/glyph.md) or a `config.app` state block):
a manifest must be readable **statically, from files, without running the app** — which is how
external tools consume it. "Consuming beats producing."

# Two tiers — both optional, with defaults

**Identity** (the "who am I" standard): `name`, `id`, `version`, `description`, and optionally
`authors`, `repo`, `homepage`, `status`. The uniform handle any tool can rely on.

**Representation** (the optional "how do I present myself" layer): a *small normative core* plus
a *free bag* — the same "known namespaces + a `free` catch-all" shape as the
[tag system](/concepts/tag-system.md). Normative: `palette.<role>`
(primary / accent / bg / ink / ok / warn / err), `icon` (a [leading-word](/references/engineering-vocabulary.md)
name), `theme` (a name). Everything else is free-form (`theme.*`, custom keys); multiple themes
allowed. The core ships **no assets** — a renderer (a holiday or the host) resolves icon names
and concrete palettes.

Frontmatter is flat (`palette.primary: "#7c3aed"`) so the existing OKF frontmatter parser needs
no change. When `app.md` is absent the reader falls back to `index.md` frontmatter, then to
`index.md`'s first heading + paragraph, then the folder name — so every bundle yields something.

# Status

`current` — the reader (`holidays/okf/manifest.py`) + Void Core's own [app.md](/app.md) are
built and tested (`holidays/okf/manifest_test.py`); `validate` exempts `type: Manifest` from the
code-`resource:` honesty rule. The **palette** half of the representation renderer is now built —
see [theme resolution](/concepts/theme-resolution.md), which turns a declared palette into a
complete, legibility-guaranteed theme (and which `validate` now uses to warn on illegible declared
palettes). The **icon/asset** half remains `planned` and belongs to a renderer holiday, not the
core. Proposed by the FaultSack agent; design + decisions in
[app-manifest design](/design/app-manifest-design.md).
