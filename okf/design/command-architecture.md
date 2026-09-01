---
type: Design
title: Command architecture
description: How the one dispatcher command surface is structured (the entry point every host shares).
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-28T00:00:00Z
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

---

# Resolved 0.2.8 — the answer, and the part of the stance that survived

Forced by **Void Palabra**, which had recorded reified commands + the
pure/effectful split as *"two blockers, both owned by Void Core"* since
2026-07-27, blocking all *utterance* work (`VoidPalabra:/concepts/utterance.md`)
— their non-linear history pillar entire. Normative text: `SPEC.md §6.2`.

**The stance's own precondition was honored:** purity was resolved *first*.

> A command is **effectful** iff its verb can reach the host through the effect
> handler. `save`, `deploy`, `build`, `preview`, `effect` — complete, because
> `vc_set_effect_handler` is the only way out of a core that does no I/O.

Two properties were not obvious before writing them down:

1. **Static, not observed.** `save` is effectful on a host with no effect handler
   registered, even though nothing left the process. The tempting design — call a
   command pure if no effect actually fired — makes the *same command* a
   recordable change on one peer and not on another, which is the precise
   divergence the distinction exists to prevent. Observation may **upgrade** a
   compound command and never downgrade one, which is how `batch` gets an honest
   answer without the classification becoming host-dependent.
2. **Effectful commands must still be recorded.** Excluding them makes `pure` a
   constant `true` and leaves a consumer unable to tell *"nothing effectful
   happened"* from *"a deploy happened and was not recorded"* — the second
   silently drops it from a replay.

**And the table above turned out to be a false choice.** It reads as memento
*versus* reified command, one replacing the other. What shipped is **both, as two
structures**, because they answer different questions and the structural
requirements are incompatible:

| | undo stack | journal |
|---|---|---|
| answers | *how do I take this back here* | *what happened, as data* |
| bounded? | **yes** — drops the oldest frame | no; a record that forgets is not a record |
| consumed by `undo`? | **yes** — frames move between stacks | no; it *gains* an entry |

A single structure cannot be both bounded-and-consumed and complete. The `swap`
the note anticipated was the wrong move; the seam it protected was still exactly
the right seam, and the journal hangs off it untouched.

**What `minted` settles.** §3.1 ids come from the PRNG, so replaying a command
*string* produces different state — which is what makes a command log alone
useless as a history, and is the thing Palabra had independently worked out
(*"the utterance records the identity that was minted rather than re-deriving
it"*). The entry carries it. It is computed by **diffing the state's id set**
across the command rather than by instrumenting `vc_mint_id`: the minting sites
are in the model layer, which has no manager to report to, and a recorder reached
through a global would break the ABI's promise that distinct managers are
independent across threads.

**Answers to the analysis owed** (§"Be careful what it MEANS", above):

1. **Railguard** — held. The journal *describes*; nothing replays it inside the
   core. Reification did not turn the core into an execution engine.
2. **Pure vs effectful** — answered, above. This was the blocker.
3. **Granularity** — `batch` is **one** entry, matching its one undo frame.
   Nestable commands were not needed to get there.
4. **Scope of the undoable slice** — still open (`SPEC.md §12`), and the journal
   deliberately does not force it: `slice` *reports* where a change landed
   (`undo` / `view` / `host`) rather than legislating what belongs where.
5. **Are reads commands?** — **no.** The journal records what changed something:
   mutations, view mutations, `undo`/`redo`, and anything effectful. A pure read
   records nothing. The RL-trajectory use would want the other answer, so if that
   work lands it needs its own recorder — noted rather than pre-built.

**Still not done:** commands have no **inverses**. Undo remains snapshot-based,
so the O(diff)-per-frame memory win in the table above was not collected, and is
not currently wanted. Nothing depends on it — `SPEC.md §12`'s remaining half is
about what `undo` should *do* with an effectful command, not about how it stores
one.

# 0.2.9 — "not currently wanted" expired, with numbers

The sentence above was true on 2026-08-27 and false the next day. Void Unity
measured the memento against a `mantles` that holds a **world** rather than a
document, and the table it produced is the argument this design note never had:

| runes | `set` µs/cmd | build (ms) |
|---:|---:|---:|
| 1 000 | 6 339 | 2 524 |
| 4 000 | 27 562 | 56 199 |

27.6 ms for one `set` is longer than a 60 Hz frame. 56 s to build 4 000 runes is
quadratic, because each `rune new` deep-copies every rune already there. The
premise that changed is theirs, not ours: *a thing is a rune iff it can be the
endpoint of a link*, which makes every door, crate, NPC and supply line in a
streaming map a rune. Every previous host kept a **document** in `mantles` —
Hormiga's topology, Maiz's surface census, Reyna's archive — and a memento of a
document is cheap. A memento of a world is the world.

**What 0.2.9 did, and what it deliberately did not.** It shipped the switch —
`vc_set_undo` / `vc_set_undo_depth`, mirroring `vc_set_journal` — and nothing
else. That is honest about what it is: the memento is still O(slice) *when it is
taken*, and turning it off buys a host the world manager it could not have, not a
cheaper undo. Measured here, at 600 runes: build **498 ms on / 18 ms off**, one
`set` **1 808 µs on / 8 µs off**.

The asymmetry it closes is the one Void Unity named: `vc_set_journal`'s own doc
comment says *"a host that does not ask for the record pays neither the entries
nor the id-diff"*, and undo is the same kind of thing — a record kept for a host
that might want to walk it back — with no switch at all. Two records, one
principle, now one rule.

**Two hosts now ask for reification**, for different reasons: Void Palabra needs a
change describable to another machine (`MESSAGE_FOR_VOIDPALABRA_reified-commands-and-purity`),
Void Unity needs one that is not a copy of the world. The journal already proves
the core can describe a change as data; what is missing is the **inverse**, which
is what would let a frame cost O(diff) instead of O(slice). That is the next
piece, and it is now wanted.

**A cheaper option Void Unity named without asking for**: make the memento
**copy-on-write per mantle** rather than duplicating the whole slice. A command
touches one mantle, so this fixes the common case, needs no new contract, and is
smaller than reification. Not built — recorded here so the choice between the two
is made deliberately rather than by whichever gets attempted first.
