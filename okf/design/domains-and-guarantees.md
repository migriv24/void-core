---
type: Design
title: Domains and guarantees
description: Where Void Core applies (the forge, not the artifact) and the guarantees every app inherits.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Domains, and the Guarantees Void Core Makes

> **Log / outline — still brainstorming.** Working out *where* Void Core applies
> (website vs. website-manager vs. game vs. game-editor) and *what every app built
> on it is guaranteed*. Companion to [what-voidcore-is-not.md](/design/what-voidcore-is-not.md).

---

## 1. The principle: the forge, not the artifact

The cleanest rule that fell out of your website analysis, generalized:

> **Void Core overlays the *authoring / management* layer of content — never the
> *runtime / consumption* layer.** It is the **forge**, not the **artifact**; the
> **studio**, not the **show**.

This is guarantee #5 ("we create content, not content itself") stated sharply, and
it resolves every domain the same way:

| Domain | The artifact (NOT Void Core) | The forge (Void Core applies) |
|---|---|---|
| Web | the **website** people visit | the **website manager** (Hormiga, BiologyManager) |
| Newsletter | the sent **email** | the **newsletter builder** |
| Game (pygame) | the **running game** the player plays | the **level / dialogue / asset editor** |
| Game (Godot) | the **exported game** | the **content manager** over scenes/resources |
| Film | the **finished video** | the **production / shot manager** |

So your point-by-point intuition was right:
1. **A website is content** → not a Void Core target. ✓
2. **A website manager makes content** → Void Core applies; it gets a CLI. ✓
3. **pygame** → the *game* is the artifact; a **pygame content manager** is the
   forge. runes = levels / sprites / entities / dialogue lines; a mantle = one
   level (or the game config); holidays = the engine, the asset pipeline, the
   export/build. Same shape as the website/website-manager split.
4. **Godot** → same: the editor/manager over scenes & resources is the forge; the
   running export is the artifact. (Godot's own `.tscn`/resource files are the
   "real backend" an adapter reads/writes — like BiologyManager rewriting HTML.)

---

## 2. "Does Hormiga wrap an abstract application?" — no

You weren't sure whether an app like Hormiga *wraps an abstract app and adds its
own stuff*, or whether *apps-on-top define their own domain stuff*. It's the
second, and there is **no separate "abstract application" to wrap**:

- **The core is the abstract substrate** (model + dispatcher + tags + DSL, in C).
- **An app is a "domain module"** that registers, in the host language:
  - its **glyphs** (the rune types it edits),
  - its **adapters** (runes ↔ its real backend),
  - its **holidays** (the external systems it touches),
  - its **tools** (the curated agent surface — see
    [tools-memory-extensions.md](/design/agent-tools-memory.md)),
  - and **its own CLI front-end** (display + interaction — itself a holiday, per
    [c-core-architecture.md](/design/c-core-architecture.md) §4).

This matches the "host + many site modules" lesson from BiologyManager's
`LEARNINGS.md`: **host = Void Core + domain modules.** Hormiga is one such host;
it does not wrap an abstract Hormiga — it *is* Void Core plus Hormiga's domain
module plus Hormiga's existing Flask/Electron harness.

---

## 3. The guarantees (logged + sharpened)

Anything built on Void Core is guaranteed:

1. **A tagging system.** Axis-typed tags + the filter grammar (`SPEC.md §5`).
   Non-negotiable; it's how everything is selected and reasoned about.
2. **An interaction-net architecture.** The app is **modeled as** an interaction
   net — runes = agents, mantle = net + rule table, holiday = boundary port
   ([interaction-nets.md](/design/interaction-nets-theory.md)).
   - ⚠️ **Honest nuance:** guaranteed *modeled as* a net now (the structure is
     stored), not *reduced as* a net yet (no reducer). "Uses interaction nets"
     means the architecture is net-shaped, not that we run graph rewriting today.
3. **Modularity.** The app is a host + domain modules (§2); pieces register into
   the core rather than being hard-wired.
4. **The rune / mantle / holiday vocabulary.** (+ spirit, glyph, domain, tag,
   binding.) The shared language every Void Core app speaks.
5. **It creates content; it is not the content.** The forge-not-artifact principle
   (§1).

### 3.1 Candidate additional guarantees (the "some more stuff" slot)
*You said you have more to add — parking likely candidates here to react to:*
- [ ] **A CLI** — every app exposes the dispatcher through some front-end
  (Codex Law 9). Guaranteed *capability*, app-defined *presentation*.
- [ ] **A logging spine** — copyable, host-routed (Codex Law 4 / `SPEC.md §9`).
- [ ] **Undo / dirty-tracking** — every mutation is reversible (`SPEC.md §6`).
- [ ] **Grammar-level capability control** — an app/agent's allowed actions are a
  narrowable grammar subset, not a runtime guard ([dsl-and-pel.md](/design/voidscript-dsl.md)).
- [ ] **Host-language independence** — the domain module can be in any language
  the FFI reaches.
- [ ] _(yours to add…)_

---

## 4. Open questions
- **Granularity of runes per domain.** A pygame level: is each entity a rune, or
  is the level one rune with structured content? (Echoes the old "one rune per
  paragraph is a lot of runes" question in `LEARNINGS.md`.)
- **How much CLI does the core scaffold vs. the app build?** A default front-end
  the app overrides, or fully app-owned? (Leaning: core ships a *reference* CLI
  front-end; apps override.)
- **Is "creating content" ever too narrow?** Are there forge-like tools that
  aren't "content" (a config manager, a data-pipeline builder)? Does the principle
  bend to "authoring/managing structured artifacts" more generally?
- **Where exactly is the orchestration-vs-computation line** for each domain (the
  open question from [what-voidcore-is-not.md](/design/what-voidcore-is-not.md) §4)?
