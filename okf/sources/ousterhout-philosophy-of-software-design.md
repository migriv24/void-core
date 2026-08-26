---
type: Source
title: Ousterhout — A Philosophy of Software Design
description: The origin of the "deep module" vocabulary the engineering-vocabulary page and the codebase-design skill are built on — and, until now, the bundle's one wholly unattributed borrowing.
resource: https://web.stanford.edu/~ouster/cgi-bin/aposd.php
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-08-09T00:00:00Z
---

# What it is

**John Ousterhout**, *A Philosophy of Software Design* (1st ed. 2018; 2nd ed. 2021),
recalled as the source of:

- **Deep vs shallow modules** — a module's value is the ratio of the functionality it
  provides to the size of the interface it presents. Depth is the goal; a class with many
  small methods that each pass through is *shallow* and earns nothing.
- **Complexity as the enemy**, decomposed into *dependencies* and *obscurity*, and
  accumulating through many small "it's only a little worse" decisions.
- **Information hiding** vs *information leakage* (the same design decision reflected in
  multiple places).
- **"Define errors out of existence"** — designing the interface so an error case cannot
  arise, rather than reporting it.
- **Tactical vs strategic programming**.

# What Void Core uses it for

[Engineering vocabulary](/references/engineering-vocabulary.md) is built on this
vocabulary — deep module, seam, depth-as-leverage — and so is the `codebase-design`
workflow skill the project adopted. The vocabulary is genuinely load-bearing: it is the
language design conversations happen in, and it shapes real decisions (the
[dispatcher](/concepts/dispatcher.md) seam, the [holiday](/concepts/holiday.md) boundary,
"one core, three faces").

**It was, until this page, cited nowhere.** The bundle used "deep module" as if it were
common vocabulary. That is the failure mode this folder exists for, and it was found by
grepping our own bundle for external names after Void Maiz raised the convention — the
terms we borrow *most* fluently are the ones least likely to carry an attribution.

# Why it is credible

A widely-read book by the author of Tcl/Tk and the Raft-adjacent Stanford systems group;
"deep module" has entered general usage largely through it. Note that its claims are
**design opinion, well argued** — not results. Citing it establishes *where the vocabulary
comes from*, not that the advice is proven.

# What a verification pass should check

`confidence:asserted` — written from recall, not checked against the book.

1. **Edition and year** (1st 2018 / 2nd 2021), and whether the 2nd edition changed any of
   the terms above.
2. **"Deep module" phrasing and definition** — confirm it is interface-size vs
   functionality, and that "shallow" is the term used for the opposite.
3. Whether **"define errors out of existence"** is verbatim.
4. Whether *seam* comes from this book at all — it may be **Michael Feathers**, *Working
   Effectively with Legacy Code* (2004). [Engineering vocabulary](/references/engineering-vocabulary.md)
   uses "seam" heavily and attributes it to nobody; if it is Feathers, that is a second
   source page, not a correction to this one.

Item 4 is the likely error: two vocabularies from different books have probably been
merged under one implicit attribution.
