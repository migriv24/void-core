---
type: Source
title: WCAG — contrast ratio thresholds and relative luminance
description: The accessibility standard `holidays/okf/theme.py` implements — and the one source here whose claims are mechanically checkable rather than a matter of reading.
resource: https://www.w3.org/TR/WCAG21/#contrast-minimum
tags: [status:current, audience:dev, confidence:asserted, source]
timestamp: 2026-08-09T00:00:00Z
---

# What it is

**Web Content Accessibility Guidelines** (W3C), recalled as specifying:

- **contrast ratio** `(L1 + 0.05) / (L2 + 0.05)`, where `L` is *relative luminance* —
  each sRGB channel normalized to 0–1, linearized (`c/12.92` below the ~0.03928 knee,
  else `((c + 0.055)/1.055) ^ 2.4`), then combined as
  `0.2126 R + 0.7152 G + 0.0722 B`;
- **1.4.3 Contrast (Minimum), level AA** — 4.5:1 for normal text, 3:1 for large text;
- **1.4.11 Non-text Contrast, level AA** — 3:1 for UI components and graphical objects;
- **1.4.6 Contrast (Enhanced), level AAA** — 7:1 / 4.5:1.

# What Void Core uses it for

[Theme resolution](/concepts/theme-resolution.md) turns an
[app manifest](/concepts/app-manifest.md)'s declared palette into a complete theme that
is **legibility-guaranteed**, and those guarantees are stated in WCAG's numbers: ink vs
background and `on_<role>` vs its role at 4.5:1, accents-as-text at 3:1. `validate` also
emits a palette-honesty warning using the same thresholds. The math is implemented from
scratch in `holidays/okf/theme.py` (pure stdlib, no color-science dependency), which means
a misremembered constant would be silently wrong in every theme the engine resolves.

One derived claim rests on this too: the resolver **always terminates**, because the better
of black/white contrasts at least √21 ≈ 4.58 against any background, so any target ≤ 4.5 is
reachable. That argument uses the ratio formula above and fails if the formula is wrong.

# Why it is credible

A W3C Recommendation — normative, versioned, and publicly checkable. Of everything in this
folder, this is the source least likely to be wrong about itself and most likely to have
been *transcribed* wrong by us.

# What a verification pass should check

`confidence:asserted` — the numbers were written from recall, then implemented.

1. **The linearization constants**: the 0.03928 (or 0.04045) threshold, the 12.92 divisor,
   the 1.055/0.055 offsets, the 2.4 exponent. An error here shifts every computed ratio
   slightly and would pass all our tests, because our tests use our own formula.
2. **The luminance coefficients** 0.2126 / 0.7152 / 0.0722.
3. **The `+ 0.05` flare term** in the ratio.
4. **Which threshold applies to what** — in particular that non-text/UI is 3:1 under 1.4.11,
   which is the rule we apply to accents.
5. **Version**: we cite WCAG 2.1. Check whether 2.2 changed any of the above, and whether
   APCA (the WCAG 3 candidate contrast model) is far enough along to matter — if it lands,
   `theme.py`'s whole approach is a version behind rather than wrong.

Item 1 is the one worth actually opening the spec for: it is the only claim here that our
own test suite structurally cannot catch.
