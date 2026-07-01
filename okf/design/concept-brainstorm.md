---
type: Design
title: Concept brainstorm (archive)
description: Archived early brainstorming of the Void Core concept vocabulary; kept for provenance.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-07-01T00:00:00Z
---

# Concept Brainstorm — Q&A Log

> A **living record** of the Socratic brainstorming about what Void Core *is*.
> Captured so we can resume the thread anywhere. These are working positions, not
> yet folded into `../SPEC.md`. When one hardens, promote it. Companion to
> [domains-and-guarantees.md](/design/domains-and-guarantees.md) and
> [interaction-nets.md](/design/interaction-nets-theory.md).

---

## The reframing that started this round

I had under-weighted the tag system. Correction, now load-bearing: because the
core treats a rune's `content` as **opaque** (`SPEC.md §3.2`), the only things the
core can *see* about a rune are its **glyph** (type), its six **facets** (text),
and its **tags**. So tags are not a convenience feature — **they are the core's
entire addressing-by-meaning layer.** A rune's `name` is just a tag that happens to
be unique. `@group:science` and `nervous-bubble` are the same mechanism at
different cardinalities. **Tags ARE how Voidscript selects anything**; tags and the
DSL are one system, exactly as the user intuited.

## The biggest revelation

> "the main inspiration for void core and its structure was for making a game
> engine." — user, 2026-06-12

This recontextualizes everything. The mantle + tags + rules design is converging on
an **Entity-Component-System (ECS)** game-engine architecture, fused with
interaction-net semantics and LLM-facing facets:

| ECS | Void Core | Notes |
|---|---|---|
| Entity | **Rune** | the thing in the world |
| Component / flag | **Tag** (membership) + facets/content (data) | tags = which systems see it |
| System (query → behavior) | **Mantle rule** | "all runes tagged X interacting with Y → effect" |
| Query (`with<A,B>`) | **Tag expression** (`@A AND B`) | this is "scripting via tags" |

This is why Deltarune (a real interactive system, not a static site) is the right
first test bed: it exercises the engine-shaped core the website managers never did.

---

## Decisions locked this round

1. **Tags are living sets, not stamped labels.** A rule over `@enemy` applies to
   every rune that *is* tagged `enemy* at evaluation time — newcomers included.
   (Q: new enemy gets prior rules → yes.) A tag is a **predicate**, not a folder.

2. **Rules are absolute; the core does not babysit.** If the user writes a dumb
   rule ("all `@enemy` spawn with double health" → their 20hp enemy becomes 40),
   that's on them. Void Core does not prevent foot-guns; it guarantees the
   *machinery*, not the wisdom of the rule set.

3. **The good-practice shape of a rule is an INTERACTION, not an edict.** Not
   "`@enemy` has double health" but "*when* a `@plant AND @weak` creature
   **interacts with** an `@enemy`, the enemy's health doubles." Void Core won't
   *force* this form, but it's the form the engine is built to reward. Interaction
   rules **compound and build off each other** — that compounding is the source of
   **emergent behavior**, the whole point.

4. **Tags relate to each other as a WEIGHTED GRAPH.** Tags aren't only attached to
   runes; they have edges *to other tags* carrying a weight/similarity ("`water` is
   1 unit similar to `rain`"). Rules can then fire on **adjacency**: "creatures with
   `water`, *or tags slightly adjacent to it*, become `swimmer` when they interact
   with a non-creature `@water` entity." → A hand-built **semantic distance space**
   over tags. (See "connections" below — this is the reachable, non-neural version
   of the [context-and-rl.md](/design/context-optimization.md) vector north star.)

5. **Rules can MUTATE tags, not just read them.** A rule's effect may *assign* a tag
   ("…will be given the tag `swimmer`"). So tags are both the query surface and a
   mutable output of rules. State lives in tags.

6. **Every Void Core app has BOTH a UI and a CLI.** The UI is a human-friendly
   wrapper over the *same dispatcher* the CLI drives. Always, regardless of the
   tool. (This generalizes `SPEC.md`'s "one core, three faces" to "the GUI is the
   first-class face for humans; the CLI is the first-class face for agents; same
   dispatcher underneath.")

7. **The forge boundary, sharpened.** Void Core is for software that **constructs a
   new, exportable artifact destined to be consumed outside the tool** — something
   that didn't exist when you started. Verdicts: DAW ✓ (a song), GIMP ✓ (an image),
   Blender ✓ (model vertices → export `.obj`), CAD ✓ (a printable model), **game
   *engine* ✓**, Deltarune mod tool ✓ (a mod others install). **Obsidian ✗** (notes
   for *you*, terminal, not shipped). **IDE = genuinely unsure** — it "makes
   software," but intuitively a poor fit; flagged as the boundary's hard case.

8. **A mantle is a NOUN (a container),** but mantles are **placed on top of each
   other**, and a stack of two mantles needs a **third mantle to define how they
   interact** (open: or do mantles carry their own inter-mantle rules?). This is
   net composition: the third mantle is the rule table for the boundary. It implies
   mantles are **higher-order / recursive** — a mantle whose runes are mantles.
   (Click LaFont on the biology site is the prior art: today that interaction is
   `bindings`; the "third mantle" idea promotes bindings to a mantle.)

---

## The co-development plan (how we harden the core)

- **Build order:** (1) an initial **C → FFI** Void Core; (2) the **Deltarune mod
  creation tool** (full UI + CLI) *on top of it*; (3) hit a wall; (4) diagnose
  whether the wall is in the core or in how we built on it. Both must be rock-solid
  before Void Core is taken to **Hormiga** (which is sensitive and should have had
  the core from the start — integrating later is the penalty for that).

- **The core/app debugging principle (agreed):** Void Core gives two faces over one
  dispatcher (human UI, agent CLI). If **everyone fails** (the AI agent *and* the
  human, across both faces) → suspect **Void Core**. If **anyone succeeds** → the
  capability exists in the core, so the failure is in a **face/app** (the UI, the
  glyph, the adapter). Formal version: *a bug reproducible through the pure
  dispatcher alone is a core bug; a bug that needs a glyph/adapter/holiday to appear
  is an app bug.* This also defines **minimum v0 core** = whatever the dispatcher
  must contain for the Deltarune tool to stand on it.

---

## Connections worth chasing

- **Weighted tag graph = a hand-built embedding space.** Edges-with-weights between
  tags is literally a low-rank semantic geometry, authored instead of learned. It is
  the *reachable today* version of [context-and-rl.md](/design/context-optimization.md)'s
  "commands/DSL as high-dimensional vectors" north star. "Slightly adjacent" = a
  distance threshold. Later, those weights could be *learned*; now they're declared.

- **The model itself is a universal interchange format (the user's Q5 dream).**
  Anything a Void Core app exports could *also* export a mantle/holiday file — a
  universal representation any other (differently-structured) software could try to
  rebuild from, because every artifact is described in the same
  rune/mantle/tag/facet vocabulary with the same fundamental laws. Cross-domain in
  principle (a 3D object and an audio file share the substrate even if not the
  bytes). Like `github.com/p2r3/convert`, but grounded in a shared structural
  semantics rather than format adapters. The **facets** (who/what/when/where/why/how)
  are what give every rune portable *meaning* regardless of domain; the **tag axes**
  (`SPEC.md §5`) are the interlingua that lets two tag sets merge by typed union.
  This elevates "interlingua" from a tag-only property to a property of the **whole
  model**. Aspirational guarantee, parked here.

- **ECS + interaction nets + tag-graph + LLM-facets** may be the one-line identity
  of Void Core's engine model. Worth testing that synthesis explicitly.

---

---

## Round 2 resolutions

**Q3 → effects are a COMMUTATIVE MONOID, and the spine is "no vicious cycles."**
The user picked (b) and tied it directly to **Yves Lafont / interaction
combinators not allowing vicious cycles.** So the mathematical root: rule effects
combine order-independently (×, +, max, union), and the rule system must be
**acyclic/terminating** — the same property that makes combinator reduction halt.
This makes "no vicious cycles" a *real constraint we must enforce*, not a slogan
(see new open question on monotonic tag-mutation).

**Q9 → facets are PURELY inert metadata. Facets and tags do NOT merge.** A rune
behaves identically whether its facets are full or empty. LLM/CLI agents *search
and read* facets (via `find`/`describe`) to understand a rune — but **rules cannot
be written against facets.** Clean split: **tags = the rule/selection surface
(machinery); facets = the reasoning/search surface (documentation).** A facet has
no mechanical effect, ever.

## What a glyph IS (Q8 — the user didn't know, so we define it)

The user can crisply separate *rune* and *mantle* but the word **glyph** "just
stuck" from early brainstorming. Grounding it:

> **A glyph is to a rune what a class is to an object** — the rune's single,
> intrinsic **type/archetype**. One glyph, many runes of that glyph.

Why it must exist *separately from tags* (it is not redundant):

1. **It carries CODE, not just a label.** A glyph bundles the app-supplied
   `editor` (how a human edits this rune in the UI), `describe` (how it
   summarizes), `newContent` (its default payload), and `render` (how it becomes an
   artifact — GML, a WAV, an HTML block). Tags are inert; **a glyph is the seam
   where the host application plugs domain code in at the rune level.**
2. **It defines the content SCHEMA.** `content` is opaque to the core; the glyph is
   what gives it a shape (`dialogue` → {character, text, expression, choices}).
   Without a glyph, `content` is a formless blob.
3. **It is singular and immutable; tags are plural and mutable.** You can't be half
   a glyph. A `walk`-action rune *is* a walk action.
4. **(interaction-net link)** the glyph fixes the rune's **arity / port shape** —
   its agent-symbol. How many things it can wire to.

**Unification that dissolves the Q8 glyph-vs-tag fork:** surface the glyph to the
rule/query system **as one reserved, immutable tag** (`glyph:walk`). Then rules
*uniformly* key on tag-expressions (the ECS/their-examples model), and
glyph-keying is just `@glyph:dialogue` — a special case, no separate engine. This
matches "a rune's name is also a tag": the rune's **name** (unique identity tag)
and **glyph** (immutable type tag) are both surfaced as tags; everything else is
free tags. **The core sees a rune as: a bag of tags, three of which are special
(name = unique, glyph = code-bearing+immutable), plus inert facets + opaque
content.** So: glyph = the rune's physical shape & code seam; tags = behavioral
eligibility; the glyph just happens to also be queryable as a tag.

Deltarune grounding: glyphs = `dialogue`, `walk`, `wait`, `freeze`, `actor`,
`battle`, … (one per cutscene action type). Each glyph's `render` emits the
matching `c_cmd(...)`. A dialogue-rune's *tags* (`chapter:2`, `susie`,
`intro-scene`) decide which rules touch it; its *glyph* decides its fields and its
GML output. Naming "glyph" is still open (the user finds it fuzzy).

## Open questions still live (for the next round)

- **NO VICIOUS CYCLES, enforced how? (the live one.)** Rules can *mutate* tags
  (Round 1 #5), and rules *compound* (#3). So rule A tags a rune `swimmer`, rule B
  reads `swimmer`, … and if B can *remove* a tag that A re-adds, you get an infinite
  loop — exactly the vicious cycle Lafont forbids. Candidate fix: make rule-driven
  tag changes **monotonic within a settling pass** (rules may *add* tags, never
  remove — a grow-only semilattice → guaranteed termination & confluence). Cost: a
  creature can't *lose* `swimmer` on leaving water within the same pass. Alternative:
  allow removal but require the rule graph be **stratified/acyclic**. Must decide.

- **Where do tag-graph weights come from, and at what scope?** Author-declared vs
  derived; per-mantle vs app-global vs core-universal. Tags live in a mantle
  (`Codex §2`), but a `water~rain` similarity feels app- or world-global. Tension.

- **Are the fundamental tag axes frozen or app-extensible?** (`SPEC.md §12`, still
  open; the axis-as-mutual-exclusion idea from Q3 may force a decision.)

- **Mantle stacking: third-mantle vs self-describing mantles.** Is the "third
  mantle" the universal mechanism for all cross-mantle interaction (retiring
  `bindings`), and does it make mantles formally recursive?
