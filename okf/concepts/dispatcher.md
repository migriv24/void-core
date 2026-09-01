---
type: Concept
title: Dispatcher
description: The one command entry point the CLI, GUI, and script runner all call; every mutation is undoable and dirty-tracked.
resource: core/src/dispatch/dispatch.c
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-08-28T00:00:00Z
---

The **dispatcher** is Void Core's single surface: one router that every face (CLI,
GUI, [Voidscript](/concepts/voidscript.md)) calls. "One core, three faces."

# Contract

Every verb returns `{ ok, lines, data }` — `lines` for humans, `data` for machines,
`ok=false` instead of throwing across the boundary. Argument parsing is quote-aware
and shared by the CLI and Voidscript.

# The command codec

A dispatcher argument carries **an arbitrary NUL-free byte string**, and
`split(quote(v)) == [v]` for every such `v` — newlines, quotes, backslashes and
control characters included. That sentence is the contract a host builds against,
and it is a *law* (`voidcore/codec_test.py` checks it as a property over generated
inputs, not a list of values somebody thought of).

Both halves ship as code — `vc_arg_quote` / `vc_argv_split_json` /
`vc_transcript_split_json` on the C ABI, `quote_arg` / `split_args` /
`split_transcript` in Python (pure, no engine) — because SPEC §6.1 spent a release
as *a specification standing in for a component*, and five independent
implementations of it got it wrong, this repo's own reference core among them.
**A rule that must be reimplemented will be reimplemented wrong.**

The **decoder** is the half hosts forget, and it is the one a
[holiday](/concepts/holiday.md)-shaped application needs most: three apps in this
family independently converged on *proposing a command transcript that a human then
dispatches*, and that review is worth nothing unless the reviewer can ask "what will
this text actually do" with the tokenizer that will do it. `split_transcript` answers
it — cutting on newline and `;` **outside** quoted runs, refusing an unterminated
quote, and reporting whether the transcript is *flat* (no blocks, no control words),
which is the property that makes its effect readable without simulating it.

An unterminated quote is an **error** (§6.1 rule 5, since 0.2.7). It used to run to
end of input, and that silence is the whole reason this bug class corrupts content
with `ok:true` rather than announcing itself.

A dispatcher instance is **not thread-safe** (one mutable state document,
unsynchronized undo/redo stacks): hosts serialize calls per instance or confine it
to one thread; distinct instances are independent. Callbacks (log sink, effect
handler) run synchronously *inside* the dispatch and must not re-enter. SPEC §6.

# Pure vs effectful, and the command journal

**0.2.8.** The design note that parked reified commands set one condition on
building them: *resolve the pure-vs-effectful distinction first* — "probably the
single most important distinction to get right"
([command architecture](/design/command-architecture.md) §2). SPEC §6.2 resolves
it and then reifies.

> A command is **effectful** iff its verb can reach the host through the effect
> handler. The complete list is `save`, `deploy`, `build`, `preview`, `effect`;
> every other verb is **pure**.

The list is complete *because* the core does no I/O — `vc_set_effect_handler` is
the only way out. And the classification is **static**: `save` is effectful on a
host that registered no handler, even though nothing left the process. A
host-dependent answer would let one command be a recordable change on one peer and
not on another, which is the divergence the distinction exists to prevent.
Observation may **upgrade** a compound command (a `batch` containing a `deploy` is
effectful), never downgrade one.

The **journal** is that classification made useful: every successful mutating
command reified as `{seq, command, verb, who, pure, slice, minted}` — read with
the `journal` verb or `vc_export_journal`, **off by default**. Undo is untouched
and stays memento-based, because the two answer different questions and want
different structures: the undo stack is *bounded* and drops its oldest frame,
and `undo`/`redo` *consume* frames. A record that silently forgets, or that loses
an entry when a change is taken back, is not a record.

Two fields carry the weight. `command` is the **canonical** line, so `rm x` and
`rune rm x` never record as two different changes. And `minted` is the ids that
exist after the command and did not before — which is what makes an entry worth
more than its own text: ids come from the PRNG, so replaying the *string*
produces different state and replaying the *entry* does not.

Effectful commands are recorded too, even though a replay consumer must skip
them. Otherwise `pure` would be a constant `true`, and nothing could distinguish
*"nothing effectful happened"* from *"a deploy happened and was not recorded."*

# POSIX surface

The command surface speaks terminal, so agents can lean on their shell priors:
**mantle ≈ directory, rune ≈ file, tag expression ≈ glob**. `cd`/`pwd`/`rm`/`mv`/
`cp`/`mkdir`/`grep`/`man` (plus `?`/`quit`/`dump`) are **argument-aware
desugarings** rewritten to canonical argv *before* routing — one semantics, many
spellings; an alias can never fork behavior, undo labels, or mutation
classification (`rm x` *is* `rune rm x`). Cold starts are self-explanatory:
root-`ls` (no active mantle) lists the mantles, and `use` with no args (or `cd /`)
deactivates back to the mantle list. Normative table: SPEC §7.1; implemented in
both the [C core](/components/c-core.md) (`args.c`) and the JS oracle.

# Invariants

- Every mutating verb pushes an undo frame before mutating; redo clears on new
  mutation; the undo stack is bounded.
- **Undo is the host's switch** (`vc_set_undo`, `vc_set_undo_depth`; 0.2.9), on by
  default, mirroring the journal's. The frame is a copy of the whole undoable
  slice, which is proportional to a *document* for a host that keeps a design in
  `mantles` and proportional to a *world* for one whose runes are live instances
  — and which of those `mantles` holds is a decision only the host has made. With
  undo off no frame is taken, and `undo`/`redo` fail *saying they are disabled*
  rather than reporting an empty stack, because a script running inside such a
  host did not strike that bargain. `batch` stays atomic either way: its rollback
  is its own saved copy, not the undo stack.
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

`current`. Implemented in the [C core](/components/c-core.md); the §6.2 journal is
`core/src/dispatch/journal.c` (conformance case 14, `bindings/python/journal_test.py`),
undo control is `core/src/dispatch/undo.c` (`bindings/python/undo_control_test.py`).
The transformation verbs
(`scry`/`temper`/`materialize`/`reduce`) are built at the Python seam, configurable by code
or by **data** (`config.transform`, `voidcore.spec`). See `SPEC.md` §6–§7. Design: [command architecture](/design/command-architecture.md).
