---
type: Concept
title: Dispatcher
description: The one command entry point the CLI, GUI, and script runner all call; every mutation is undoable and dirty-tracked.
resource: core/src/dispatch/dispatch.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-06-28T00:00:00Z
---

The **dispatcher** is Void Core's single surface: one router that every face (CLI,
GUI, [Voidscript](/concepts/voidscript.md)) calls. "One core, three faces."

# Contract

Every verb returns `{ ok, lines, data }` — `lines` for humans, `data` for machines,
`ok=false` instead of throwing across the boundary. Argument parsing is quote-aware
and shared by the CLI and Voidscript.

# Invariants

- Every mutating verb pushes an undo frame before mutating; redo clears on new
  mutation; the undo stack is bounded.
- Dirty-tracking compares against a `_baseline` snapshot (`status`/`diff`/`revert`).
- `batch` applies a list of commands **atomically** (rollback on any failure) as **one
  undo frame** — which is how the seam gives a multi-write transform pass
  (`temper`/`materialize`/`reduce --commit`) a single author-facing undo.

This single, fully-described surface is what makes the core **agent-drivable**: an
LLM calls dispatcher verbs, never internals.

# Transformation verbs (the seam)

The three transformation layers ([Reduce](/concepts/reduce.md) /
[Temper](/concepts/temper.md) / [Scry](/concepts/scry.md)) are exposed as dispatcher verbs
— `scry`, `temper`, `materialize`, `reduce` — through a **seam**:
`voidcore.Dispatcher`, a drop-in **superset** of `VoidCore.dispatch` that handles those
verbs and delegates every other command to the [C core](/components/c-core.md) unchanged.
The layers are implemented **once** (the tested Python modules) and surfaced here rather
than duplicated into C — so they're part of the dispatcher *contract* (any binding may
implement them) with Python as the reference impl. Mutating transform verbs write back
through `setjson`/`tag`, so they remain undoable. See `SPEC.md` §7 "Transformation verbs".

# Status

`current`. Implemented in the [C core](/components/c-core.md); the transformation verbs
(`scry`/`temper`/`materialize`/`reduce`) are built at the Python seam, configurable by code
or by **data** (`config.transform`, `voidcore.spec`). See `SPEC.md` §6–§7. Design: [command architecture](/design/command-architecture.md).
