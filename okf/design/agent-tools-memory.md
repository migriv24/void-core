---
type: Design
title: Agent tools, memory, extensions
description: Agent tool structure, memory, sandbox, and extension design tracks.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-07-01T00:00:00Z
---

# Tools, Memory, Sandbox, and Extensions

> Grab-bag of four related design questions. The connective theme: **keep the core
> small; let capability grow through well-bounded mechanisms.**

---

## 1. Agent tools — few in the core, many in the app

Recent practice suggests **~40 well-polished tools** is a sweet spot for a single
agent; thousands is possible but only with context management (see
[context-and-rl.md](/design/context-optimization.md)). Two layers, kept distinct:

- **Dispatcher verbs** (`SPEC.md §7`) — many, fine-grained, the *substrate*
  (`set`, `tag`, `rune new`, `bind`, `describe`, …). ~40 already.
- **Agent tools** — few, polished, *task-level*. A tool = `{ name, description,
  arg-schema, body }` where the body is a Voidscript snippet / template over
  dispatcher verbs. Tools are the curated, documented surface an agent chooses
  from; verbs are the raw machine underneath.

Design stance:
- **Void Core itself ships only 3–5 core tools** + a **tool-definition framework**.
  Candidate core tools: `look` (read/describe state), `do` (run a command/script),
  `find` (tag-query), `compose` (run a Voidscript), and maybe `molt` (summarize
  context). Everything else is app-defined.
- **Every app builds its own ~40 tools** from the substrate (Hormiga's will be very
  different from BiologyManager's). The core does not guess them.
- **Don't hard-cap tool count**, but default to a curated set. Scaling to
  thousands is a *retrieval* problem — tools are taggable (reuse the tag system!),
  surfaced by relevance, not all loaded at once.
- A tool's availability is also a **capability**: the enabled tool set ⇔ the
  enabled grammar subset ([dsl-and-pel.md](/design/voidscript-dsl.md) §3.1). Tools and
  grammar-level safety are the same lever from two angles.

---

## 2. Memory — reuse the model, don't build an engine

Two kinds, both already expressible:

- **Working memory** = the **molt / summary** from
  [context-and-rl.md](/design/context-optimization.md). Session-scoped, compressed, transient.
- **Durable memory** = **runes**. A memory is a rune in a `memory` mantle: it has
  `facets` (who/what/when/where/why/how — already perfect for a memory record),
  `tags` (recall by tag-query), and a `spirit` (stable id). Recall = `find` /
  `ls --tag`. We get a queryable memory store **for free** from the existing model.

So: **no separate memory subsystem.** Working memory is the context scaffold;
durable memory is a mantle of memory-runes. (This also mirrors how this very
assistant's file-based memory works — facets ≈ frontmatter, tags ≈ the index.)

---

## 3. Sandbox — mostly dissolved by grammar-level safety

PEL's lesson ([dsl-and-pel.md](/design/voidscript-dsl.md) §1): if the **grammar can't express
a forbidden action, you don't need to sandbox it.** Most of what a runtime sandbox
would guard against is removed *proactively* by narrowing the grammar / disabling
verbs and tools per capability.

The residual, genuine sandbox need is at **one place: the holiday boundary.**
Holidays are exactly the parts that touch the world we don't control — network,
filesystem, deploy, external APIs ([interaction-nets.md](/design/interaction-nets-theory.md): a
holiday is a boundary port). So:

- **Language core**: safe by grammar. No general-purpose sandbox.
- **Holiday boundary**: real guardrails — allow-lists, dry-run, explicit
  confirmation for outward/irreversible effects, rate limits. Concentrate *all*
  sandbox-like effort here, where it actually matters.

This is a clean separation and it keeps "sandbox engine" from ballooning scope:
there is no engine, there is a **guarded boundary**.

---

## 4. Extensions — disambiguate two meanings, then design one

You flagged the ambiguity yourself: *doesn't every piece of software "extend" Void
Core?* Yes — so split the words:

- **Application / manager** = software built *on top of* Void Core (Hormiga,
  BiologyManager). Not an "extension."
- **Extension** = a packaged addition to the **core itself** that *any* application
  then inherits. This is the word we reserve.

**Extension mechanism (sketch).** A registrable bundle that may contribute:
glyphs, dispatcher verbs, grammar rules, holidays, tools, or REPeL behaviors —
opt-in per app. It generalizes the existing **glyph registry** pattern to the whole
core surface. Pairs naturally with tool-retrieval and context management
([context-and-rl.md](/design/context-optimization.md)): extensions are *how the core stays small
while capability scales* — which is why you intuited building them "around the same
time as RL-based context management." Agreed.

### 4.1 The "character" extension (e.g. *Jacob*) — a great first extension
A registered **character** with a name; calling it returns dialogue. It adds
*flavor*, not function — which is *exactly* why it belongs as an extension rather
than in the core. Two reasons it's worth doing soon:

1. **It's the perfect test of the extension API.** A character is a small bundle:
   a glyph (`character`), a verb (`say <name>`), and some state (lines, mood). If
   the extension mechanism can cleanly add a Jacob that any app inherits, the
   mechanism is sound.
2. **There's prior art: Click.** BiologyManager's *Click LaFont* is already a
   character — implemented bespoke as a "mantle on top of" each site. A **character
   extension generalizes Click** into a reusable, core-level thing. So this isn't a
   toy detour; it's the abstraction Click was gesturing at, done right. Build the
   character extension, and Click becomes "just a configured character."

> Net: extensions = the scalability valve. The character extension is both the fun
> idea you like *and* the cleanest way to prove the extension API before anything
> load-bearing depends on it.
