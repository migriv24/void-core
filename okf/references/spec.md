---
type: Reference
title: SPEC.md — the normative contract
description: The indexed entry point to SPEC.md, Void Core's normative language-agnostic contract, mapping each section to the concept that describes it.
resource: SPEC.md
tags: [status:current, audience:dev, audience:library, confidence:verified, reference]
timestamp: 2026-07-01T00:00:00Z
---

**`SPEC.md`** (at the repo root) is Void Core's **normative, language-agnostic
contract** — what *any* implementation (the [C core](/components/c-core.md), the JS
oracle, a future binding) must do. Where the [concepts](/concepts/index.md) explain
*what a thing is and why*, SPEC states *the exact obligations*. This page is the OKF's
indexed doorway into it; the canonical text stays in `SPEC.md` (not duplicated here).

Section markers in SPEC: **[impl]** = mandatory in the reference impl · **[ext]** =
Hormiga-only extension · **[seam]** = dispatcher contract implemented once at the Python
seam (optional per binding) · **[impl unless marked]** = mandatory verbs except where noted.

# Section → concept map

| SPEC section | What it fixes | Concept |
|---|---|---|
| §1 What Void Core is | the overlay-not-runtime boundary | [what Void Core is NOT](/design/what-voidcore-is-not.md) |
| §2 The state document `[impl]` | the serialized model / source of truth | [rune](/concepts/rune.md), [mantle](/concepts/mantle.md) |
| §3 Data model | spirit, rune, glyph, mantle, domain, binding, links | [rune](/concepts/rune.md) · [glyph](/concepts/glyph.md) · [mantle](/concepts/mantle.md) · [domain](/concepts/domain.md) · [links](/concepts/links.md) |
| §4 Identity & reference rules | frozen id + editable name; ref resolution | [rune](/concepts/rune.md) |
| §5 Tag system `[impl]` | tag membership + the filter grammar | [tag system](/concepts/tag-system.md) |
| §6 Dispatcher contract `[impl]` | the one command entry point + result shape | [dispatcher](/concepts/dispatcher.md) |
| §7 Verb catalog | verb semantics incl. the transformation verbs (seam) | [dispatcher](/concepts/dispatcher.md) · [reduce](/concepts/reduce.md) / [temper](/concepts/temper.md) / [scry](/concepts/scry.md) |
| §8 Voidscript `[impl]` | the scripting language over the dispatcher | [voidscript](/concepts/voidscript.md) |
| §9 Adapter seam & logging `[impl]` | the I/O boundary + log spine | [holiday](/concepts/holiday.md) · [logging & debug](/concepts/logging-debug.md) |
| §10 Extensions for Hormiga `[ext]` | host-specific extensions (not in reference impl) | — |
| §11 Conformance | the JS oracle as conformance reference | [c core](/components/c-core.md) |
| §12 Open spec questions | undecided contract points | [roadmap](/roadmap.md) |

# Status

`current` — SPEC.md v0.1 tracks the built [C core](/components/c-core.md). When a
[design](/design/index.md) track matures into an obligation, it lands in SPEC first,
then here.
