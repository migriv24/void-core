---
type: Design
title: Command architecture
description: How the one dispatcher command surface is structured (the entry point every host shares).
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Command Architecture in the Core — Build Now, Analyze Later

> **Flagged for deeper analysis** (user, 2026-06-14): *"we can have some kinda
> commander based architecture within the core itself, though we do have to be a
> bit careful on what it MEANS to have that. let's have this BUT document it as
> something we need to further analyze later."*
>
> This note records what we built (v0 undo/redo) and the open question of turning
> mutations into **first-class command objects**. Companion to `../SPEC.md §6`,
> [dsl-and-pel.md](/design/voidscript-dsl.md), [context-and-rl.md](/design/context-optimization.md),
> [interaction-nets.md](/design/interaction-nets-theory.md).

---

## What we built (v0, shipped in the C core)

**Memento-based undo/redo** (`core/src/dispatch/undo.c`):
- Each *mutating* verb snapshots the undoable slice (`mantles` + `active`) **before**
  running, labelled by the command string.
- On success the snapshot commits to the undo stack and the redo stack clears; on
  failure it's discarded (no junk history, redo preserved). Stack bounded at 200.
- `undo [N]` / `redo [N]` move frames between stacks by swapping snapshots;
  `history [--tail N]` lists the labels. Matches SPEC §6 exactly.

The **insertion seam** is already isolated: `is_mutating()` + `vc_undo_capture()` /
`vc_undo_commit()` in `dispatch.c`. Swapping the mechanism later touches only this
seam — the verbs themselves don't know how undo works.

## The bigger idea: reified commands ("the commander")

A **command object** would make each mutation a first-class value:
`{ verb, args, (inverse | snapshot), effects }` — instead of an opaque before-image.
Two mechanisms sit on a spectrum:

| | **Memento (now)** | **Reified Command (proposed)** |
|---|---|---|
| Stores | a before-snapshot | the operation + how to invert it |
| Undo | restore snapshot | apply the inverse |
| Memory | O(state) per frame | O(diff) per frame |
| Per-verb cost | none (generic) | each verb defines its inverse |
| Introspectable? | only the label | fully (verb + args as data) |
| Composable / replayable? | no | yes |

## Why it's tempting (the connections)

Reified commands are the same object several other tracks want:
- **Voidscript / homoiconicity** ([dsl-and-pel.md](/design/voidscript-dsl.md)) — a command *is*
  a program fragment; code-as-data makes undo, scripting, and metaprogramming one
  thing.
- **RL trajectories** ([context-and-rl.md](/design/context-optimization.md) §3) — every dispatch
  is an `(observation, action, outcome)` tuple. A command log *is* the trajectory
  record, for free, if commands are structured data.
- **Interaction-net rules** ([interaction-nets.md](/design/interaction-nets-theory.md)) — a mutation
  as a small local rewrite; a command and a rule-firing converge in shape.
- **Universal interchange** (the Q5 dream in
  [concept-brainstorm.md](/design/concept-brainstorm.md)) — a command log is a portable,
  replayable description of *how an artifact was made*.

## "Be careful what it MEANS" — the analysis owed

1. **Railguard.** Reified commands must stay **orchestration descriptors**, not
   become an execution/computation engine ([what-voidcore-is-not.md](/design/what-voidcore-is-not.md) §4).
   A command says *what to do to the model*; it does not *compute the app's output*.
2. **The holiday boundary breaks undo.** Pure model mutations are reversible; a
   command that fires a **holiday/adapter** (save, deploy, an external write) is
   **not** snapshot-undoable (SPEC §12 open question). A command model must tag each
   command **pure vs effectful**, and only pure ones are undoable. This is probably
   the single most important distinction to get right.
3. **Granularity.** Is `batch` one command (one undo frame) or many? Are compound
   edits one reversible unit? The reference impl treats `batch` as atomic — that
   argues for **nestable/compound commands**.
4. **Scope of the undoable slice.** Today: `mantles` + `active`. Should `config`,
   `domains`, `scripts` be undoable too? (Probably not config/domains — they're
   setup, not content.)
5. **Are reads commands?** For RL traces, *every* dispatch is an action; for undo,
   only mutations are. The command abstraction may need to span both with a
   `mutates: bool`.

## Stance for now
Keep memento (correct, SPEC-aligned, cheap). **Do not** reify commands yet — but
keep the `is_mutating`/capture/commit seam clean so the swap stays local. Revisit
when Voidscript or the RL-trace work makes commands-as-data pay for itself, and
resolve the **pure-vs-effectful** distinction *before* building it.
