---
type: Reference
title: SPEC.md — the normative contract
description: The indexed entry point to SPEC.md, Void Core's normative language-agnostic contract, mapping each section to the concept that describes it.
resource: SPEC.md
tags: [status:current, audience:dev, audience:library, confidence:verified, reference]
timestamp: 2026-08-31T00:00:00Z
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
| §6 Dispatcher contract `[impl]` | the one command entry point + result shape; mutation invariants, incl. **host-controlled undo** (`vc_set_undo`/`vc_set_undo_depth`, on by default — a memento of a document is cheap, a memento of a world is the world); §6.1 = **argument quoting** — the tokenizer's five rules, the law `split(quote(v)) == [v]`, and the exported codec; §6.2 = **pure vs effectful + the command journal** — which verbs cross the holiday boundary, and every mutating command reified as data | [dispatcher](/concepts/dispatcher.md) |
| §7 Verb catalog | verb semantics incl. the transformation verbs (seam); §7.1 = the POSIX alias surface (argument-aware desugarings, root-`ls`, `cd /`) | [dispatcher](/concepts/dispatcher.md) · [reduce](/concepts/reduce.md) / [temper](/concepts/temper.md) / [scry](/concepts/scry.md) |
| §8 Voidscript `[impl]` | the scripting language over the dispatcher; **a CR is a line terminator, not content** (so a CRLF-authored script means what it reads as); §8.1 = **quoting and expansion** — the rules that keep a value in a transcript from becoming syntax | [voidscript](/concepts/voidscript.md) |
| §9 Adapter seam & logging `[impl]` | the I/O boundary + log spine | [holiday](/concepts/holiday.md) · [logging & debug](/concepts/logging-debug.md) |
| §10 Extensions for Hormiga `[ext]` | host-specific extensions (not in reference impl) | — |
| §11 Conformance | the JS oracle as conformance reference; a runner MUST deliver cases **byte-for-byte** — a suite that normalizes its own inputs is not testing what a host will be handed | [c core](/components/c-core.md) |
| §12 Open spec questions | undecided contract points | [roadmap](/roadmap.md) |

# Status

`current` — SPEC.md v0.2 tracks the built [C core](/components/c-core.md). When a
[design](/design/index.md) track matures into an obligation, it lands in SPEC first,
then here.
