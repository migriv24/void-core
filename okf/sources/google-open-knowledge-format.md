---
type: Source
title: Google Cloud — the Open Knowledge Format
description: The external format this entire bundle claims conformance to; the one source whose artifact is also a reference implementation we can diff against.
resource: https://cloud.google.com/blog/products/data-analytics/how-the-open-knowledge-format-can-improve-data-sharing/
tags: [status:current, audience:dev, audience:library, confidence:asserted, source]
timestamp: 2026-08-09T00:00:00Z
---

# What it is

**Google Cloud's Open Knowledge Format (OKF)** — knowledge as a directory of markdown
files with YAML frontmatter — announced on the Google Cloud blog, with a reference
implementation at [GoogleCloudPlatform/knowledge-catalog](https://github.com/GoogleCloudPlatform/knowledge-catalog)
(`okf/SPEC.md`, an enrichment agent, and sample bundles each carrying a self-contained
`viz.html`).

# What Void Core uses it for

Everything. [OKF spec](/references/okf-spec.md) restates the format,
[OKF as a core feature](/design/okf-design.md) argues for adopting it, the
[OKF engine](/components/okf-engine.md) implements consume/produce/validate against it,
the [glossary](/references/voidcore-glossary.md) is the OKF ⇄ Void Core dictionary, and
**this bundle is the engine's hand-authored conformance fixture** — so a
misunderstanding of the format is a misunderstanding of our own test data.

The specific claims we restate as OKF's rules: only `type` is required;
`title`/`description`/`resource`/`tags`/`timestamp` recommended; extra keys allowed;
Concept ID = path minus `.md`; `index.md` and `log.md` reserved; and **permissive
consumption** (tolerate unknown types, unknown keys, missing optional fields, and broken
links).

# Why it is credible

It is the format's own publisher, and unusually for this folder the artifact is *code as
well as prose* — the reference implementation can be read and diffed, so conformance is
checkable rather than interpretive.

# What a verification pass should check

`confidence:asserted` — restated from recall, and this one has drifted furthest from its
artifact simply by being built on for months.

1. **Version.** We claim conformance to **v0.1**. Check whether that is the version number
   the spec uses and whether it has moved.
2. **The reserved-file list.** We treat `index.md` and `log.md` as reserved and exclude
   them from the concept set (`bundle.py` `RESERVED`). Confirm both are OKF's, not ours —
   `log.md` in particular feels like a Void Core habit.
3. **Frontmatter key names**, especially `resource:` and whether OKF spells it that way.
   Our whole freshness/drift lint keys off it.
4. **`okf_version` in `index.md`** — we say `index.md` is "the only place frontmatter is
   allowed, for `okf_version`", which is oddly specific and worth confirming.
5. **Whether "permissive consumption" is OKF's stated rule** or our inference. We lean on
   it hard — it is the justification for `type:` tags, free tag axes, the `notes` split,
   and Void Maiz's whole surface-census vocabulary riding on unknown types.
6. Whether the format defines anything about **links** (we treat markdown body links as the
   graph, which is load-bearing for the citation convention on this very page).

Item 5 is the one to check first: several downstream designs were approved *because*
permissive consumption made them safe.
