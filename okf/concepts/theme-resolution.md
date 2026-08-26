---
type: Concept
title: Theme resolution
description: The renderer that turns a manifest's declared palette into a complete, legibility-guaranteed theme — the concrete-palette half of the representation layer the app manifest deferred to "a renderer".
resource: holidays/okf/theme.py
tags: [status:current, audience:dev, audience:library, confidence:verified]
timestamp: 2026-07-04T00:00:00Z
---

The [app manifest](/concepts/app-manifest.md) lets an app declare a representation **palette**
(`palette.primary`, `palette.bg`, …) but deliberately renders nothing: *"the core ships no
assets — a renderer (a holiday or the host) resolves icon names and concrete palettes."*
**Theme resolution is that renderer** for the palette half (icons stay `planned`).

# The problem it solves

A *declared* palette is sparse and can be hostile. An app might declare a near-white `ink` over
a light `bg`, or omit `ink` entirely — and a consumer that paints those colors verbatim (as
FaultSack, the external OKF study tool, did when theming study sites to the app under study) gets
white-on-white text. Responsiveness to an app's brand shouldn't cost legibility.

# What `resolve_theme(palette)` guarantees

It turns a declared palette into a **complete** color set (17 roles, always all present) where
every text/background pair meets **[WCAG AA](/sources/wcag-contrast.md)**:

- body text (`ink` vs `bg` and vs the card surfaces) — **4.5:1**;
- accents and semantic colors as text (`primary`/`accent`/`ok`/`warn`/`err` vs `bg`) — **3:1**
  (AA for large text / UI components);
- each `on_<role>` (text painted on top of a filled accent) vs its role color — **4.5:1**.

It also derives a light/dark **`mode`** from the background, and the surface/border overlays a
card system needs. A palette that is **already compliant passes through byte-identical** (the
app's brand is untouched, `adjustments == []`); a non-compliant one is nudged the *minimum*
amount toward black or white, and every change is recorded in `adjustments` for honesty.

# Why it always succeeds

For any opaque background, `contrast(bg, white) × contrast(bg, black) = 21`, so the better of
black/white always contrasts at least √21 ≈ 4.58 with it. Any target ≤ 4.5 is therefore always
reachable by mixing a foreground toward that anchor — the resolver never asks for more than 4.5
on a guaranteed pair, so it can't fail to terminate. Pure stdlib WCAG math; no color-science
dependency.

# Role vocabulary

Input roles (all optional): `primary, accent, bg, ink, ok, warn, err` (the manifest's normative
palette roles). Output `colors` keys: `bg, surface, surface_strong, border, ink, ink_dim,
ink_faint, primary, on_primary, accent, on_accent, ok, on_ok, warn, on_warn, err, on_err`.

# The validator hook

[validate](/components/okf-engine.md) warns when a *declared* palette pair (ink/bg, accent/bg)
renders illegibly, or when a `palette.<role>` isn't valid hex. It only checks pairs where **both**
colors are explicitly declared — sparse palettes are filled by the resolver by design, not a
mistake. The message says renderers will auto-correct, so an app that wants to own its brand
knows to declare a compliant color instead of relying on the fallback.

# Status

`current` — `holidays/okf/theme.py` (`resolve_theme`, `contrast`, `ensure_contrast`, …) built
and tested (`holidays/okf/theme_test.py`: WCAG math, the anchor guarantee, hostile-palette fix,
compliant pass-through, idempotence, validator warnings). Consumed by FaultSack, which resolves
the studied app's palette at ingest and themes its study site from the result. Icon/asset
resolution remains `planned`.
