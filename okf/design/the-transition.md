---
type: Design
title: The transition (2026-10) — pillars, and where development begins
description: "IMPORTANT — read first. The author's ruling of 2026-10-30: Void Core enters a transitional phase. Two new pillars — world models (optimization shifts from LLMs toward JEPA-class models, LLM compatibility kept) and the database (a dedicated library above Core, Void Rancho). The four aspects of Void Core ranked, the raw structure first. Separating the command language from the structure is recorded, not started."
tags: [status:current, audience:dev, confidence:asserted, pillar, foundation]
timestamp: 2026-10-30T00:00:00Z
---

# The transition (2026-10)

> **This page is a ruling, not a proposal.** Its *direction* is current; the work it
> points at is planned. Any session starting Void Core development reads this first, and
> the first two items in [the roadmap](/roadmap.md) are its two pillars.

## 1. The ruling

The author, 2026-10-30, after the [world models](/design/world-models.md) research:

> *"we want to start transitioning optimization from llms to jepa models."*
>
> *"database management is crucial to hormiga, and it will be for any other application …
> the database logic itself has been an afterthought, when in reality its one of the most
> important parts of any application … what would a database look like to a jepa model? i
> think it would look very similar to our runes and mantle structure!"*

Two pillars follow, and they are one idea seen from two sides: **a mantle is the shape both
a learned world model and a database want.** Entities with identity across time
(`spirit.id`), typed relations between ordered pairs (`layout.edges`), typed numbers
([quantity](/concepts/quantity.md)), and transitions labelled by the action that caused
them (the journal). The JEPA literature spends its effort recovering those from pixels
([world models](/design/world-models.md) §4); a database spends its effort storing and
indexing them. Void Core already defines them.

## 2. The four aspects of Void Core, ranked

| rank | aspect | what it is | where it lives |
|---|---|---|---|
| **1** | **the raw structure** | rune, mantle, glyph, tags, edges, quantities, the state document — the thing being described | SPEC §2–§5, `core/src/model`, `glyph`, `tags` |
| 2 | the command language | the nouns and verbs: dispatcher, CLI, Voidscript, undo, journal | SPEC §6–§8, `core/src/dispatch`, `scripts` |
| 3 | the OKF | self-description: every app's knowledge bundle | `okf/`, `holidays/okf` |
| 4 | multi-language support | the C ABI, the bindings, the §6.1 codec | `core/include/voidcore.h`, `bindings/` |

The ranking is the author's, and it settles a question this bundle left implicit: **when
the structure and the command language disagree about what is convenient, the structure
wins.** Everything else — including every model and every database — reads the structure.

## 3. Pillar A — world models

Optimization shifts **from LLMs toward JEPA-class world models**, with LLM compatibility
kept (an LLM is one actor among several; [LLM-JEPA](https://arxiv.org/abs/2509.14252) shows
the two meet). The full research, the integration architecture and the phased changes are
in [world models](/design/world-models.md). Its findings that now bind design:

- The bias toward LLMs is in what Core says **only in prose** — the action space, a
  command's effect, a transition — not in the command language as such.
- The pure/effectful line is also the **exact/learned** line: Core knows its own dynamics
  exactly; a learned model predicts what crosses the effect boundary.
- **Running models is Void Bicho's job.** Core only has to allow it.
- `spirit.id` random and never content-derived is load-bearing for a third reason.

## 4. Pillar B — the database: Void Rancho

Every application on Core has built its own database: Void Hormiga a SQLite store with a
cluster-paged working set (2026-10-27, after a client's 5×10⁵-rune database peaked at
22 GB), Void Squid an `assets.db` copying Hormiga's schema, Void Loops Studio a project
that is a mantle. **The model was shared; the database never was.**

The ruling: a **dedicated library one layer above Core**, the way Void Palabra and Void
Maiz are — **not inside Core**, which does no I/O by definition and stays pure. It is
named **Void Rancho** (`../VoidRancho`, its own OKF), and it is distinct from Hormiga's
Antfarm: the Antfarm connects a database to external APIs in real time; Rancho *is* the
database. Rancho's founding direction, from the author: generalize what Hormiga built;
take clustering and **edge weights as floats** seriously; plan for **matrix math over runes
and mantles** so a mantle can be read as the arrays a JEPA consumes; and design for **large
data at speed** — network drives, a NAS, object storage.

What Core owes Rancho is small and is the same thing it owes world models: say what a
command **touched** (world models phase 1). Hormiga's store already writes back after every
dispatch by re-asking Core with `cat` and `links`, because the journal records the action and
not its effect. One additive change serves both pillars.

## 5. Recorded, not started — separating the command language from the structure

The author, 2026-10-30: *"the separation of the CLI is important too, but given how
fundamental it is, and how difficult it might be to separate, let's just note it."*

It has come up three times:

1. **2026-06-12**, [C core](/design/c-core-architecture.md) §4, *"The CLI is a holiday"*:
   the REPL, rendering and input are host adapters; the core provides command *semantics*.
   That separated the **terminal** from the core. It did not separate the **command
   language** from the **structure** — both still live in one library.
2. **2026-09-23**, Void Hormiga relaying the author: *"maybe the void core needs to be …
   broken into 2 parts? the pure 'rune, mantle, holiday' structures, and the CLI?"* Their
   lean: a pure half that is **read-only** (project, query, verify a state document), so
   every change still goes through the dispatcher.
3. **2026-10-28**, [world models](/design/world-models.md) §10.1: a world model needs only
   *read* and *propose*, which is the same shape.

Why it is hard: undo, the journal, the codec, the ref index and the tag grammar are all
reached through dispatch today, and every host's write path is commands. Why it may be
worth it: rank 1 above says the structure is the most important thing, and today it is
reachable **only** through rank 2. Not started; the read-only lean is the default if it is.

## 6. Where development begins

In order, once the author starts:

1. **Void Rancho's OKF** — founded 2026-10-30 (`../VoidRancho/okf`).
2. **World models phase 1 in Core** — the verb catalogue as data, and `touched` on the
   journal. Serves Rancho's write-back and a world model's transitions at once.
3. **Rancho rung 1** — Hormiga's store schema, extracted and generalized, with its
   model-based test against a Core holding everything.
4. **The first world-model experiment** (Bicho's) — a model of the dispatcher, ranked
   against exact dry-run outcomes.
