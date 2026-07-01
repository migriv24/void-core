---
type: Reference
title: Open Knowledge Format v0.1
description: The external format this bundle conforms to — markdown + YAML frontmatter, only `type` required, cross-links form a graph.
resource: https://cloud.google.com/blog/products/data-analytics/how-the-open-knowledge-format-can-improve-data-sharing/
tags: [status:current, audience:dev, audience:library, confidence:verified, reference]
timestamp: 2026-06-18T00:00:00Z
---

**Open Knowledge Format (OKF) v0.1** — Google Cloud's open spec for representing
knowledge as a directory of markdown files with YAML frontmatter. The format this
bundle conforms to, and the protocol the [OKF engine](/components/okf-engine.md)
speaks. Void Core's own rationale for adopting it: [OKF as a core feature](/design/okf-design.md).

# Essentials

- A **Knowledge Bundle** is a directory tree; each non-reserved `.md` is a
  **Concept** whose **Concept ID is its path minus `.md`**.
- Frontmatter: only **`type`** is required; `title`/`description`/`resource`/`tags`/
  `timestamp` recommended; any extra keys allowed.
- Concepts cross-link with markdown links (bundle-root-relative `/…` preferred),
  forming a graph richer than the directory tree.
- Reserved files: `index.md` (listing; the only place frontmatter is allowed, for
  `okf_version`) and `log.md` (dated history).
- **Permissive consumption**: tolerate unknown types, unknown keys, missing optional
  fields, and **broken links** (not-yet-written knowledge). Only stale/dead
  `resource:`→code links are a freshness signal for us.

# Citations

[1] [How the Open Knowledge Format can improve data sharing](https://cloud.google.com/blog/products/data-analytics/how-the-open-knowledge-format-can-improve-data-sharing/)
[2] [GoogleCloudPlatform/knowledge-catalog](https://github.com/GoogleCloudPlatform/knowledge-catalog) — reference impl (`okf/SPEC.md`, enrichment agent, sample bundles each with a self-contained `viz.html`)
