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

# Source provenance — a local convention

OKF says nothing about citing external work, and a bundle written by an agent needs a
rule, because *a surname, a year and a theorem name are exactly what comes out fluent and
can be wrong*. Once written, a bad attribution is invisible: it reads correctly, it is
never re-derived, and nothing checks it. (Convention adopted from Void Maiz, 2026-08-07.)

Three parts, none of which needs an engine change:

1. **One concept per external work**, `type: Source`, under `sources/`. It records what
   the work is, what *we* use it for, why it is credible, and **what a verification pass
   should check** — the last section being the one that makes the page actionable rather
   than decorative.
2. **Cite with an ordinary body link, never a frontmatter key.** A `cites:` key would be
   *accepted* (unknown keys are tolerated) and would be **invisible**, because the graph
   is built from body links. Citing in the body buys two things:
   `okf get sources/<name>` lists every citing page under `linked from` — and that list is
   the **blast radius**, naming exactly which pages inherit an error — while `okf analyze`
   scores an over-leaned-on source as high-centrality with no new machinery. *A bibliography
   tells you what was cited; a graph tells you what breaks.*
3. **`confidence:` already means the right thing** — no new vocabulary:
   `exploratory` = unsure the attribution is right at all; `asserted` = written from
   recall, **not** checked against the artifact; `verified` = someone opened it and
   confirmed author, year and claim. So `okf query "confidence:asserted source"` *is* the
   verification queue.

`Source` is kept distinct from `Reference` deliberately: a Reference is material *we*
wrote to be referred to (this page, the [glossary](/references/voidcore-glossary.md)), a
Source is an external work we are **trusting**. Only the second kind can be wrong in a way
we would not notice. Note that `Source` is also *not* in `validate`'s doc-type exemption,
so a source with no `resource:` is flagged — a citation that cites nothing is exactly the
thing worth catching.

# Citations

See [sources/](/sources/index.md) for how external work is cited here. This page's own:

- [Google Cloud — Open Knowledge Format](/sources/google-open-knowledge-format.md), covering
  both the announcement and the `knowledge-catalog` reference implementation.
