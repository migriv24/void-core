---
type: Roadmap
title: Roadmap
description: Forward-looking index of what is still planned and the near-term build order. Dev-only; excluded from the shipped library bundle. Completed work lives in log.md, not here.
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-08-25T00:00:00Z
---

The forward-looking counterpart to the bundle `index.md` (current listing) and
[`log.md`](/log.md) (history): an index of what is **planned** and the order we intend
to build it. This is `audience:dev` — the shipped library OKF (`status:current AND
audience:library`) leaves it out.

**Convention: this file lists only what is *not yet done*.** When something ships it
moves to [`log.md`](/log.md) and out of here; partially-done items stay, with the
remaining slice stated precisely. This index mirrors `okf query "status:planned"`.

# Remaining on built layers

The three [transformation layers](/design/transform-layers.md) are **built** and folded
into the [dispatcher](/concepts/dispatcher.md) as verbs (`scry`/`temper`/`materialize`/
`reduce`), configurable by code or by data (`config.transform`, `voidcore.spec`). What
remains is integration polish, per the concept pages:

1. **[Scry](/concepts/scry.md)** — context-parameterized `resolve` over a *live*
   [holiday](/concepts/holiday.md) **snapshot** (the `scry(state, snapshot, context)`
   shape). The pure projection/law/`materialize` pieces are done.
2. **[Temper](/concepts/temper.md)** — remaining author-facing polish on rules-as-mantle
   **data** (the `voidcore.spec` compiler exists; the ergonomics of authoring/editing them
   in a mantle are the open part).
3. **[Reduce](/concepts/reduce.md)** — general (sub-pattern / tag-expression) rule LHS
   *without* the confluence guarantee, and data-form `expand` (needs a custom build fn, so
   it stays code-registered today).

# Planned concepts

* [Links](/concepts/links.md) - the passive-connection substrate (partly built as
  `layout.edges`; the fuller loose-connection model remains)
* [Interaction nets](/concepts/interaction-nets.md) - the formalism is decided and its
  **executor is [Reduce](/concepts/reduce.md)** (built); the remaining pieces are the
  general-rule extensions above
* [UI / UX](/concepts/ui-ux.md) - abstract today; open question is *how* an app declares
  its UI/UX, plus a first renderer/representation holiday over the
  [app manifest](/concepts/app-manifest.md)
* [Logging & debug](/concepts/logging-debug.md) - richer debug/trace tooling and a
  Voidscript-driven **test engine** (`assert`-over-dispatcher)
* [Host extension seams](/design/host-extension-seams.md) - two host-callback
  registrations that ride the existing spine: **verb macros** (host `compile(argv) ->
  [command lines]`, dispatched as a `batch` — pure/undoable, distinct from the effect
  seam) and **pluggable `where`-predicates** for Scry/`ls` (host `predicate(rune, args)`,
  composed with the tag grammar). Stance recorded in the design note; graduates to
  `SPEC.md` when a second consumer appears
* [The effect seam & the session boundary](/design/effect-seam-and-session-boundary.md) -
  four contract questions routed Core-ward by Void Maiz (2026-08-18) and parked for
  review: **an effect handler cannot fail a dispatch** (`res_make(1)` precedes every
  handler call, so a failed deploy returns `ok:true` — invisible to exactly the caller
  that automates; their top ask, with a backward-compatible fix), **`save` snapshots
  `_baseline` even when the adapter failed** (a data-loss path if `_baseline` means
  "what reached the backend"), **stating the session boundary** in SPEC (history and
  the log are session-scoped; `_baseline` is model content), and **making the
  derived-id hash normative**. Stances recorded in the design note; each graduates to
  `SPEC.md` when decided
* [OKF engine](/components/okf-engine.md) - v0.1 (consume/produce/validate) is built;
  exposing it — and the built [graph-analytics](/concepts/graph-analytics.md) holiday — as
  [dispatcher](/concepts/dispatcher.md) verbs in the core remains (today both are
  host-side holidays)

# BEGIN HERE — the transition (2026-10-30)

**Important, by the author's ruling: development starts with these two pillars.** Rationale
in [the transition](/design/the-transition.md).

1. **Pillar A — world models.** Shift optimization from LLMs toward JEPA-class models
   ([world models](/design/world-models.md); track below). First Core work: the verb
   catalogue as data + `touched` on the journal.
2. **Pillar B — the database: Void Rancho.** A dedicated library above Core
   (`../VoidRancho`, OKF founded 2026-10-30). Core's part is the same `touched` change.
3. **Recorded only:** separating the command language from the raw structure
   ([the transition](/design/the-transition.md) §5).

# On hold — from the 2026-10-28 inbox

Larger or contract-shaped asks, **held by the author's instruction** while the easy fixes
shipped as 0.2.15 ([log](/log.md)). Each names who asked; ★ marks a second requester, the
family's bar for building.

1. **Reified commands with inverses** — O(change) undo instead of the O(slice) memento. One
   `set` at 10⁵ runes still costs 0.48 s. Void Hormiga (10-25) ★ Void Unity ★ Void Maiz
   (`replace_state` must drop undo history) ★ [world models](/design/world-models.md) phase 3.
2. **`save` doubles the document** — `_baseline` is a full copy of `mantles` inside the state,
   so every saved file and every export carries the database twice. Hormiga proposes a digest
   or the journal `seq` at save; `revert` would replay instead. Behavior change for
   `status`/`diff`/`revert` — see also [effect seam](/design/effect-seam-and-session-boundary.md).
3. **What changed, as data** — `touched` ids on the journal entry, `mutated` in the envelope,
   `vc_export_mantle`, or a `vc_revision` counter. Hormiga ★ Maiz (built its own conservative
   revision because "which verbs mutate is Core's to say"). Any new export turns Void Unity's
   ABI audit red, so new symbols ship together in one deliberate release.
4. **A verb catalogue as data** — the classification four hand-kept lists encode today
   (`help`, `is_mutating`, `is_effectful`, `keeps_ref_index`). Needle's tool set ★ Maiz ★
   world models phase 1. Unblocks (3).
5. **Graph analysis in C** — Brandes/label propagation live only in Python; Maiz now ships a
   C++ Leiden. Hormiga asks for a ruling: core or view library.
6. **Typed ports at the door** — `link --relation i:j` accepts an index past a glyph's
   declared ports and `validate` says `valid` (Hormiga 09-22; asked of Maiz too, whose port
   vocabulary it is). Named port relations would also stop a port reorder silently rewiring
   every database.
7. **Should Core split** into a pure read-only half and the command language? The author's
   question, relayed by Hormiga 09-23, with their lean: read-only half, every change through
   the dispatcher. Same shape world models want.
8. **Where an act rune's role list lives** — a `roles` key beside `fields`/`kinds`? (Void Maiz
   Q26; their lean: schema, not presentation.) And Maiz's (b): a renderer should not act on a
   descriptor's `source`; a schema editor should (undeclare only what was declared).
9. **A Rust crate and the ABI-stability promise** a `-sys` crate may rely on (Void Jelly
   10-03). Unity's answer — lenient late binding plus an unbound-export audit — is the
   template on offer.
10. **`batch` sub-commands on its journal entry** (Void Palabra, standing since 08-27) ★
    world models (a batch is a macro-action). And Palabra's `slice` value distinguishing
    `mantles` from `active`.
11. **The name "Void Core"** for Latin-OS's kernel — the author's ruling; nothing moves
    until then.
12. **Small, outside this repo:** Latin-OS's `void.json` uses `link: "c-abi"`, which Mago's
    schema rejects (the last `mago doctor` error); a generated `core/sources.cmake` would
    spare Maiz's Android build hand-syncing Core's file list (offered, not asked); Allomone's
    `glyph "x"` is an alias for its `kind "x"`, a second door into the `kind` word collision.

# World models track

Direction proposed 2026-10-28 in [world models](/design/world-models.md); **gated on the
author**. Running models is Void Bicho's; these are only what Core must allow.

1. **Phase 1** — verb catalogue (on-hold 4); `touched` + `mutated` (on-hold 3).
2. **Phase 2** — `batch --dry`: an exact counterfactual from the memento, no ABI change.
3. **Phase 3** — reified commands (on-hold 1).
4. **Phase 4** — cost as data: `validate --goal '<expr>'`, `diff --against <state>`.
5. **First experiment** (Bicho's to run): a world model *of the dispatcher*, ranked against
   exact dry-run outcomes, before any world model of a host.

# The open-application track

Direction set 2026-07-04 in [the open application](/design/open-application-design.md);
**gated on Miguel's answers to its §8 questions**, then built in phases:

1. **Phase A — contracts as pages** (no code): `host bundle` concept + reserved
   `host.md`/`type: Host`; the **application standard** (3-tier instantiation
   checklist); **two seats** (builder vs operator agents); manifest `ui.*` keys +
   the L0–L3 sandbox-surface ladder; engine-reuse standard + the **mantle capsule**
   envelope.
2. **Phase B — probe holiday** (`holidays/host/`): the local/host OKF as regenerable
   probed facts (first consumer: Hormiga's platform checks).
3. **Phase C — sandbox surface L0/L1**: `render`/`snapshot` effect ops + one
   reference surface; consumed by FaultSack's sandbox tab.
4. **Phase D — capsules + registry**: `mantle export`/import with policy; the tagged
   holiday registry (below) as the discovery layer; `validate --app` lint.

# Deferred / research

* **Latin-OS (external, far horizon)** — a Void Core-based operating system
  (`../LatinOS/okf/`, its own project + OKF; first named in Void Maiz's horizons):
  compositor scene graph as a mantle, no central scheduler, holidays as the
  inter-device/inter-client protocol, HVM2 as candidate root runtime. **Not a Void
  Core build target** — everything it needs is already on this roadmap for other
  reasons (open-application phases, C-ABI reduce, host probe). Its one standing
  constraint on us, in force now: **keep the [holiday](/concepts/holiday.md)
  protocol-shaped** — no design may assume a single device, a single client, or a
  central scheduler ("does this still work when the other end of the edge is
  another person's machine?"). Its HVM2-backed implementation of the Reduce
  contract (`conformance/reduce/`) would double as our C-ABI-reduce answer if it
  pans out.
* **Inter-application communication** — a protocol layer for the multiple apps built on
  Void Core to talk to each other (capability discovery, message/event passing, shared
  vocabulary) *on top of* the [OKF](/components/okf-engine.md), which already gives them a
  shared self-description. Includes the "update other agents" pattern (done by hand in the
  [transform-layers handoff](/design/transform-layers-handoff.md)) as a first-class concept.
  **Shelved on purpose (2026-06-28)** — promising, but the transformation-layer follow-ups
  come first.
* Lightweight **NLP / embedding holiday** — semantic link-suggestion + context-length
  optimization ([context optimization](/design/context-optimization.md)).
* A tagged **holiday registry** — many holidays selected by tag/capability with fallback
  chains (extends [Holiday](/concepts/holiday.md)).
