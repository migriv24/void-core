---
type: Design
title: Voidscript as a DSL (PEL direction)
description: Reconceiving Voidscript as a safe orchestration DSL: grammar-level safety, homoiconicity, pipes, self-healing.
tags: [status:planned, audience:dev, confidence:exploratory]
timestamp: 2026-07-01T00:00:00Z
---

# Voidscript as a DSL — PEL as the Theoretical Foundation

> Source: *"Pel, A Programming Language for Orchestrating AI Agents"* (Behnam
> Mohammadi, 2025). We treat Voidscript (`../src/scripts/voidscript.js`,
> specified in `../SPEC.md §8`) as a **domain-specific language** and use PEL as
> the design north star.

---

## 1. What PEL gets right (and why it fits Void Core)

PEL is a Lisp-flavored language **designed to be written by LLMs and safely
interpreted by a host** — almost exactly Void Core's CLI/agent goal. Its pillars:

1. **Grammar-level safety.** PEL's grammar is a small, regular EBNF. Because it is
   small, you can **delete rules to remove capabilities** (no network, no file
   I/O, no specific functions) and use *constrained generation* so the LLM
   *cannot even produce* a forbidden construct. Safety is enforced at the
   **syntax** level, not by a runtime sandbox. → This is a direct answer to our
   "do we need a sandbox?" question (see [tools-memory-extensions.md](/design/agent-tools-memory.md)).
2. **Homoiconicity** (code = data, S-expressions). Programs are trivially
   inspectable/generatable by other programs — and a regular symbol structure is
   the natural substrate for embedding (see [context-and-rl.md](/design/context-optimization.md)).
3. **Pipes (`▷`, `^`).** Linear left-to-right composition. PEL's key insight:
   LLMs generate **token by token, forward**; `(foo a) ▷ (bar ^)` lets the model
   commit to `foo` first and decide to pipe into `bar` *after the fact*, with no
   backtracking. Pipes align with how generation actually works.
4. **Everything is a function.** No special forms — `if`, `case`, `for`, even
   `def` are (non-strict) closures. One uniform rule for the LLM to learn.
5. **Natural-language conditions.** A string in a `case`/`if` is evaluated by an
   LLM against the scrutinee: `(case profile ["is a premium member" (...)])`.
   Fuzzy judgment as a first-class control-flow primitive.
6. **REPeL: restarts + self-healing.** Errors don't crash. The environment is
   *preserved before the error* and the user (or an automated **helper agent**) is
   offered restarts: rewrite the expression, rewrite from the error forward, abort,
   or **LLM self-heal** (an agent fixes the faulty snippet using the failing
   function's docstring as context). Crucial when expensive prior steps (API
   calls) must not be discarded.
7. **Automatic parallelization.** A static pass builds a dependency graph over the
   ASTs (what each `def` defines vs. uses); independent ASTs run concurrently.
   Immutability makes the analysis sound.

---

## 2. Where current Voidscript stands

Voidscript today is **imperative and line-based**: each line is a dispatcher
command plus shell-style tokens, wrapped in control flow (`let / if / while /
foreach / def / try-catch / on error`). It works, BiologyManager uses it, and it
is honestly quite LLM-friendly for *simple* sequences.

What it is **not** (yet), relative to PEL:

| PEL pillar | Voidscript today | Gap |
|---|---|---|
| Grammar-level capability control | ad-hoc; no formal grammar | **no EBNF, no constrained-generation gating** |
| Homoiconic / code-as-data | no (string-token AST) | hard to metaprogram or embed |
| Pipes | no | composition is via `let` + reuse |
| Everything-is-a-function | partial (control flow is built-in keywords) | not a uniform model |
| NL conditions | no | conditions are command-truthiness or operator exprs |
| Restarts + self-healing | `try/catch` + `on error stop/continue` only | **no restart menu, no helper-agent autocorrect** |
| Auto-parallelization | no | sequential |

---

## 3. Proposal: a "Voidscript v2" research track (additive, not a rewrite)

Do **not** rip out today's Voidscript — Biology depends on it and the simple
surface has merit. Treat PEL alignment as incremental upgrades, in rough priority:

1. **Write a formal EBNF grammar for Voidscript first.** Even for the current
   surface. This is the keystone: it enables (a) constrained generation, (b)
   **grammar-level capability control** — an app/mantle declares which verbs and
   constructs are enabled, and the grammar is narrowed accordingly. A "read-only
   agent" literally cannot generate `deploy` or `bind`. This is the safety story.
2. **Add pipes.** `ls --tag month:june ^> tag ^ +featured` — compose dispatcher
   commands linearly. Cheap, high-leverage for agent ergonomics.
3. **Upgrade error handling to REPeL-style restarts + self-healing.** We already
   have `try/catch` and a clean `{ok, lines, data}` result and a logging spine —
   the substrate is there. Add: preserve state on failure, present restart options,
   and a (later, optional) LLM helper-agent that proposes a fix from the verb's
   help text. This is *also* the natural home for RL later (the fix policy).
4. **Natural-language conditions** behind a holiday/LLM seam: `if "the newsletter
   has enough events" { ... }`. Only when an LLM holiday is wired in.
5. **Homoiconic / S-expression core** — the biggest change, deferred. It is the
   bridge to the vector future and to the interaction-net substrate
   ([interaction-nets.md](/design/interaction-nets-theory.md): NeLA compiles a friendly surface
   to a net — Voidscript could compile to the same net semantics). Decide later
   whether to migrate the surface or keep two front-ends.

---

## 4. Critical tensions (decide deliberately)

- **S-expressions vs. the current shell-ish surface.** Homoiconicity wins for
  metaprogramming and embedding; the current surface is arguably *easier* for an
  LLM to emit for one-liners. We may want **both**: a friendly surface and a
  canonical AST it desugars to (NeLA's two-layer move). Don't pick under pressure.
- **"We're making our own compiler."** True, and we should own it: grammar →
  parser → AST → evaluator over the dispatcher. The interaction-net model is a
  candidate **semantics** for that AST. A DSL with a real grammar is a real
  compiler; treat it with that seriousness, but keep the grammar *small* (PEL's
  entire point — a hundreds-of-lines grammar like Python's defeats constrained
  generation).
- **Don't let LLM-friendliness erase human-friendliness.** The Codex requires a
  human enjoy the CLI too. PEL manages both via simplicity; so should we.
