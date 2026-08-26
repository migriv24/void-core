---
type: Reference
title: Engineering vocabulary
description: The shared design + naming vocabulary Void Core builds with — deep modules, seams, depth-as-leverage, and the "leading words" naming principle. Names the discipline the codebase already follows.
tags: [status:current, audience:dev, confidence:asserted, reference]
timestamp: 2026-06-29T00:00:00Z
---

A small, shared vocabulary for **designing** Void Core and **naming** things in it. These
terms aren't new rules — they name the discipline the codebase already follows, so author
and agent reach for the same concept with one word. Adopted from
[Matt Pocock's "Skills For Real Engineers"](/sources/pocock-skills-for-real-engineers.md)
(MIT), whose design vocabulary in turn traces to
[Ousterhout's *A Philosophy of Software Design*](/sources/ousterhout-philosophy-of-software-design.md)
— the origin of *deep module* — and kept in line
with how this project already thinks about knowledge: [OKF](/references/okf-spec.md) (Google's
minimally-opinionated, producer/consumer-independent format) and Karpathy's *LLM-wiki* frame
(the bundle is a compounding artifact; agents do the bookkeeping; `validate` is the *lint*).

# Deep-module vocabulary

Use these exact words when designing or restructuring code (don't substitute "component",
"service", "API", "boundary"):

- **Module** — anything with an interface and an implementation; scale-agnostic (a function,
  a holiday, a whole tier).
- **Interface** — *everything* a caller must know to use it correctly: the signature **plus**
  invariants, ordering, error modes, purity, performance. (Wider than "API"/"signature".)
- **Implementation** — what's inside. Distinct from **adapter**, which names the *role* a
  concrete thing plays at a seam.
- **Depth (as leverage)** — how much behaviour a caller/test exercises per unit of interface
  they must learn. **Deep** = lots of behaviour behind a small interface; **shallow** = the
  interface is nearly as big as the implementation (avoid).
- **Seam** *(Feathers)* — the place where behaviour can be swapped without editing there; the
  *location* a module's interface lives. Say **seam**, not "boundary" (DDD-overloaded).
- **Adapter** — a concrete thing satisfying an interface at a seam.
- **Leverage / Locality** — what depth buys: leverage for callers (capability per unit
  learned), locality for maintainers (change/bugs/knowledge concentrate in one place).

## How it maps onto Void Core

- The [dispatcher](/concepts/dispatcher.md) is the canonical **deep module**: one
  `{ok, lines, data}` interface over a large behaviour set.
- A [holiday](/concepts/holiday.md) is an **adapter** at the I/O **seam**; the
  [Dispatcher](/concepts/dispatcher.md) seam adds the transform verbs as a drop-in superset.
- [Reduce](/concepts/reduce.md) / [Temper](/concepts/temper.md) / [Scry](/concepts/scry.md)
  are deep: small verb surface, much pure behaviour; a [Lens](/concepts/scry.md) hides a
  whole bidirectional mapping behind `forward`/`backward`/`check`.

## Two heuristics worth keeping

- **The deletion test.** Imagine deleting a module. If complexity vanishes, it was a
  pass-through; if it reappears across N callers, it was earning its keep.
- **One adapter is a hypothetical seam; two is a real one.** Only introduce a seam when
  something actually varies across it. (Void Core's data seam is *real*: LocalJSON, MeshDB,
  and the OKF engine are three adapters of it.)

# Leading words (the naming principle)

A **leading word** is a compact concept already in a model's pretraining that the agent
*thinks with* — naming a thing with one recruits priors the model holds, anchoring a whole
region of behaviour in the fewest tokens. It pays off twice: fewer tokens, and a sharper
hook the agent (and human) hangs reasoning on. This is the same lever Google/Karpathy bank
on for the knowledge bundle — shared language so the agent spends fewer tokens thinking and
names things consistently.

Void Core is built from leading words on purpose: **rune**, **mantle**, **holiday**,
**glyph**, **scry** / **temper** / **reduce**, **materialize**, **seam**. Guidance when
adding a new glyph, verb, or concept:

- Prefer a word that already carries the right prior over a coined or generic one
  (`scry` over "project-and-resolve"; `holiday` over "external-adapter-port").
- One concept, one word — keep it the **single source of truth** across SPEC, code, and this
  bundle (the [glossary](/references/voidcore-glossary.md) is the OKF⇄Void Core seam).
- Reuse the word everywhere it applies so its distributed meaning compounds.

# Provenance

Vocabulary adapted from Matt Pocock's `codebase-design` and `writing-great-skills` skills
(MIT), installed for this project under `.claude/skills/`. Framing kept consistent with the
[OKF spec](/references/okf-spec.md) and Karpathy's LLM-wiki pattern.
