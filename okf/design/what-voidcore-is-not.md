---
type: Design
title: What Void Core is NOT — the railguard
description: The boundary that keeps Void Core an overlay, not a language/runtime; the drift test vs Bend/NeLA.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# What Void Core Is NOT — the Railguard

> Prompted by comparing Void Core against **Bend** (`HigherOrderCO/bend`) and
> **NeLA** (`heikowagner/nela-lang`, cloned at `../../nela-lang`). Both are built
> on Lafont's interaction nets — the same theory we mine in
> [interaction-nets.md](/design/interaction-nets-theory.md) — yet **neither is the kind of thing
> Void Core is.** Writing the distinction down so we don't drift into it.

---

## 1. The corner we are avoiding: "a language with a runtime"

| | Bend | NeLA | **Void Core** |
|---|---|---|---|
| What it is | a general-purpose **programming language** | a **programming language** "for LLMs, not humans" | a host-language-agnostic **overlay** |
| Runtime | **HVM2** — its own parallel interaction-combinator evaluator | a tree-walking interpreter + a C interaction-net reducer (`.nelac` bytecode) | **none of its own** — it does not execute the app |
| You write… | your whole program *in Bend* | your whole program *in NELA-S* | your app in **any** language; Void Core sits *over* it |
| Purpose | automatic parallelism (CUDA-like compute) | formally-verifiable program generation | let an agent/human **interlace with every aspect of an existing app** |
| Host interop | none — its own isolated world | I/O harness only (Python is "strictly I/O") | **the entire point** — adapters bridge to any host |
| It **computes** | yes — reduces programs to values | yes — reduces nets to values | **no** — it orchestrates and structures; the host computes |

Bend and NeLA are *worlds you write your application inside*. They take source,
compile it to a net, and **reduce the net to compute a result** (sort a list, run
a game loop, do parallel math). That is a magnificent thing — and it is **not**
Void Core.

---

## 2. What Void Core actually is

> "Something to build on top of, with scripting languages to allow agents to
> interlace themselves with every aspect of some application — and the
> application can be built with any other language." — the user, 2026-06-12

Void Core is the **agent-and-human-facing control & structure layer over software
that already exists, in whatever language it exists in.** Concretely it provides:

- a **structural vocabulary** — rune / mantle / domain / holiday / tag — for
  describing *what an application is made of and how its parts relate*;
- a **CLI + scripting DSL** (Voidscript) so an operator can read, reason about, and
  act on the app through one surface;
- a **logging / undo / tag spine** shared across that surface.

It does **not** replace, compile, or run the application. The application can be:

- an **HTML/JS site** (BiologyManager edits source files),
- a **Python/Flask app** (Hormiga — see `Hormiga/VOIDCORE_INTEGRATION.md`),
- a **C++ game**, a **Rust CLI**, anything.

The bridge between Void Core's abstract model and the real, in-some-language app is
the **adapter** (and the **holiday** at the boundary, where it reaches systems it
does not own). The host language is **irrelevant to Void Core's model** — only the
adapter has to speak it. This host-language-agnosticism is now a first-class
principle (added to `SPEC.md §1`).

---

## 3. Where interaction nets fit — theory, not runtime

This is the subtle line, and the one most likely to blur:

- **Bend/NeLA** use interaction nets as an **execution model**: a net *is* the
  computation; reducing it *is* running the program.
- **Void Core** borrows interaction nets as a **structuring and reasoning model**:
  runes are agents, a mantle is the net + its rule table, a holiday is a boundary
  port ([interaction-nets.md](/design/interaction-nets-theory.md)). We use the *shape* — agents,
  ports, local rules, confluence, linearity — to give a principled account of *how
  the parts of an app relate*. We do **not** reduce a net to compute the app's
  output.

If we ever build a reducer, it reduces **the overlay's own relationships** (e.g.
resolving a binding, settling a layout, materializing a holiday query) — never the
host application's computation. Sorting the user's list is the host app's job, in
the host app's language. Void Core describes that the list *exists*, how it's
*tagged*, and *who may act on it* — it does not sort it.

---

## 4. The drift test

A single question keeps us honest:

> **Are we making Void Core compute the application's actual results?**

If yes — if Void Core starts sorting the lists, running the game logic, doing the
parallel math — we have drifted into Bend/NeLA territory and should stop. Void
Core **orchestrates and structures**; the **host app computes**. The scripting DSL
exists to *direct* and *compose* actions over the app, not to *be* the app's
implementation language.

> Corollary for the DSL ([dsl-and-pel.md](/design/voidscript-dsl.md)): Voidscript should stay
> an **orchestration** language (compose commands, query, react), resisting the
> gravitational pull toward becoming a general-purpose computation language. PEL
> is the right reference precisely because it is an *orchestration* language too —
> not Bend.
