---
type: Source
title: Pocock — Skills For Real Engineers
description: The MIT-licensed skill set adapted into `.claude/skills/`; the only source here that carries a licence obligation as well as an attribution.
resource: https://github.com/mattpocock/skills
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-08-09T00:00:00Z
---

# What it is

**Matt Pocock's "Skills For Real Engineers"** — a public, **MIT-licensed** collection of
agent workflow skills (recalled as including `codebase-design`, `tdd`, `diagnosing-bugs`,
`grilling`, `writing-great-skills`).

# What Void Core uses it for

Six skills were installed under `.claude/skills/` on 2026-06-29 and *lightly adapted*, so
that their domain-vocabulary pointers also read this bundle. The design and naming
vocabulary was folded into
[Engineering vocabulary](/references/engineering-vocabulary.md) — including the **leading
words** naming principle — kept consistent with the [OKF spec](/references/okf-spec.md).

Two skills were deliberately **not** taken (`grill-with-docs`, `domain-modeling`): they
impose a CONTEXT.md / ADR layout that overlaps OKF, and the project's standing decision is
that OKF is the knowledge format, not a CONTEXT.md. The issue-tracker/triage process
machinery was skipped for the same reason.

# Why it is credible

A public repository under a permissive licence by a well-known TypeScript educator. Its
authority is *practitioner experience*, not research — the skills are conventions that
work, and adopting them is a taste decision the project made deliberately, not an appeal
to evidence.

# What a verification pass should check

`confidence:asserted` — recalled, not re-read. This one has an obligation attached, which
makes it the cheapest to get wrong and the most awkward to be wrong about.

1. **The licence.** We record MIT. Confirm it, and confirm the MIT terms are actually being
   met for the adapted copies in `.claude/skills/` — MIT requires the copyright notice and
   licence text to travel with substantial portions. Adapted-but-derived files still count.
2. **Attribution of specific ideas.** "Leading words" is credited to this source in
   [Engineering vocabulary](/references/engineering-vocabulary.md); check it is Pocock's
   term rather than one we coined while reading.
3. **The skill list** — which six were installed, and whether the upstream names still
   match (a renamed upstream skill makes our adaptation harder to trace).
4. **Whether `codebase-design`'s vocabulary is itself borrowed** from
   [Ousterhout](/sources/ousterhout-philosophy-of-software-design.md). If so, the deep-module
   attribution belongs there and this page should not carry it — two of these five source
   pages may currently claim the same idea.

Item 1 is a licence question rather than an accuracy one, so it outranks the rest.
