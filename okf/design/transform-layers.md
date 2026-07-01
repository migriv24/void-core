---
type: Design
title: Reduce / Temper / Scry — design
description: The three transformation layers (graph rewrite / normalization / projection): names, forks, and the resolved design.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Reduce / Temper / Scry — Void Core's three transformation layers

> A *thinking document* (design), not the contract. Names **locked 2026-06-28**.
> Sources: Void Core's own [interaction-nets.md](/design/interaction-nets-theory.md) +
> [command-architecture.md](/design/command-architecture.md), and field input from four apps
> building on the core (Void Loops/DAW, Fountain, Portfolio Manager, Hormiga). Status:
> `planned` — this records the agreed shape + the open forks before we build.

## The naming

Data structures are **nouns** (rune, mantle, holiday, glyph, link, tag). The
*transformations over them* are **verbs** — three of them, deliberately distinct:

| verb | layer | what it does | who needs it most |
|---|---|---|---|
| **reduce** | the reducer | interaction-net graph rewrite: fire rules / expand references until **normal form** | DAW, Fountain |
| **temper** | normalization | pure clean-up of **owned** state to a **canonical form** (enforce invariants, fix derived fields) | Portfolio Manager |
| **scry** | projection / resolution | pure **views** over state + holiday **snapshots** + **context** (selectors; `resolve`; the round-trip law; `materialize`) | PM, Hormiga |

Why three names and not one "reducer": the four apps were using "reducer" for two—really
three—different machines. Splitting the word prevents building one thing that
disappoints half of them. **reduce** is the interaction-net executor the notes already
reserved; **temper** and **scry** are new sibling layers.

## reduce — the graph rewriter (see [interaction-nets.md](/design/interaction-nets-theory.md))

Fire a mantle's interaction rules / expand references on the graph until no rule
applies, producing a **derived** mantle (source untouched). The "structure compiler":
abstract rule-/tag-/reference-bearing mantle → flattened mantle a holiday can consume.
Stays on the right side of the drift test ([what-voidcore-is-not.md](/design/what-voidcore-is-not.md) §4):
it normalizes the *overlay's own structure*, it never computes/runs the host's domain output.

## temper — normalization to canonical form

Pure rules run (eagerly, cheaply) after an action to keep **owned** state consistent:
derived-field defaults (e.g. "thumb = images[0]"), de-duplication, tag normalization
(e.g. "status/year are namespaced tags, everything else is a free tag"). Centralizes the
invariants apps currently hand-code on every mutation path. `temper` is idempotent:
`temper(temper(x)) == temper(x)`.

## scry — projection & resolution

Pure **read-side**. Two shapes the apps asked for, unified here:
- `scry(state, context) -> view` — selectors over owned state (records, `ls --tag`, the
  OKF projection). Context = `{locale, audience, date/now, role}` → one mantle, many views.
- `scry(state, snapshot, context) -> view` — resolve **holiday-backed** data from a
  *snapshot* (never folded into authoritative state at edit time). Same (state, snapshot,
  context) ⇒ byte-identical output (live-preview == final render == archived send).
- The **tag-expression evaluator** is the shared primitive here, conformance-tested
  across impls.
- **Round-trip law** for any scry that backs persistence: `unscry(scry(x)) == x`,
  property-tested. (This is the bug Portfolio Manager shipped — a lossy projection
  silently dropping tags. The law makes that class structurally impossible.)
- `materialize` — the *one* explicit, undoable action that freezes a scryed projection
  back into owned state (archival "baking"). Never silent. Distinct from reduce's
  transient reference-expansion.

## Consensus invariants (unanimous across all four apps — bank as hard rules)

1. **Pure**: model→model, no I/O, no clock, no RNG. (command-arch: pure-vs-effectful is
   "the single most important distinction".)
2. **Effects live at the holiday boundary; none of reduce/temper/scry ever fires one.**
3. **Functional / derived** — the source mantle is never mutated in place.
4. **Determinism**: any nondeterminism (IDs!) is **injected by the action**, never
   generated inside. (Fountain landmine: their `secrets.token_hex` ID minting must never
   leak into reduction, or confluence dies.)
5. **Identity is a cheap, first-class default** — a rule-less mantle reduces/tempers to
   itself; `scry` of plain state is just its records.
6. **Atomic, labeled, previewable (dry-run), undoable** — one author-facing undo frame
   even for a multi-step pass.
7. **Coexist with the dispatcher; verbs are actions.** Don't replace the verb surface.
8. **Tag-expression evaluation is one shared, tested primitive.**

## Open forks (NOT yet decided — need a call before building)

1. **Cycle policy + confluence scope.** DAW has genuine feedback cycles (delays,
   sidechains) and needs them **preserved as opaque irreducible nodes** (detect SCCs,
   normalize *around* them). This contradicts interaction-nets.md's current "well-formed
   ⇒ no vicious cycles / unique normal form" assumption. Likely resolution: opaque-SCC
   pass-through, and **guarantee confluence only on the acyclic/terminating fragment.**
2. **Rule LHS generality vs confluence.** DAW wants tag-expression / sub-pattern LHS;
   Fountain needs guaranteed unique normal form. Interaction-net confluence comes
   *precisely* from the restricted one-rule-per-glyph-pair principal-port form. Can't
   promise both. Likely: support general matching, **guarantee** confluence only on the
   restricted subset, ship a **confluence/conflict validator** for general rule sets.
3. **`reduce` signature.** Plain `state -> state` (DAW/Fountain/PM) vs Hormiga's
   `(state, action) -> {state, effects[]}` (effects declared as data, runtime performs).
   Both pure; decide where the effects vocabulary rides.
4. **expand vs materialize.** reduce's reference-expansion is **transient** (regenerated
   each pass, in the derived output); `materialize` is a **durable, explicit** owned-state
   write. Support both, named distinctly — never conflate.
5. **Scheduling per layer.** temper wants to run **eagerly** (per action); reduce wants to
   be **explicit + previewable** (pre-compile); scry runs **on read/render**. Don't force
   one schedule — it falls out of the three-layer split.

## Non-contradiction worth noting

Fountain feared the DAW wanted reduce to be a *live execution engine*. Reading the DAW's
actual input, it stays a **structure compiler** (reduces relationships, not audio) — so
both want structure normalization, not running the artifact. The only real difference is
*when* reduction runs (fork #5), which is scheduling, not semantics.

## Forks RESOLVED — Reduce built 2026-06-28

The five open forks above were resolved as follows, and Reduce was built on this shape
(`VoidCore/reduce/`). The resolutions follow directly from the banked consensus invariants
and the decided interaction-net formalism (`interaction-nets.md`), so no semantics were
invented — the open questions collapsed once the invariants were applied.

1. **Cycle policy + confluence scope.** Only **active pairs** reduce — two agents wired
   **principal-to-principal** with a registered rule. Everything else is inert, so feedback
   cycles (DAW) are preserved *for free* (they're simply not redexes). The host may also
   declare agents **opaque** (by glyph or id) to freeze them explicitly. Confluence +
   termination are **guaranteed only on the terminating fragment**; a `max_steps` guard
   raises `ReduceError` on runaway (γδε is Turing-complete — termination is the rule
   author's responsibility). Auto-SCC freezing was rejected as fragile; explicit
   opaque-marking is what the DAW case actually wants.
2. **Rule LHS generality vs confluence.** Built the **restricted confluent subset**: ≤1
   rule per *unordered glyph pair*, principal-principal, local — exactly Lafont's form,
   where strong confluence holds **by construction**. Uniqueness is enforced *structurally*
   at registration (a duplicate glyph-pair raises). General sub-pattern / tag-expression
   LHS is **deferred**; when added it will not carry the confluence guarantee (a conflict
   validator will flag it). So the guarantee is honest: it holds precisely where the math
   gives it.
3. **`reduce` signature.** `reduce(net) -> net`. A **pure structure compiler**; it never
   emits effects (invariant #2). Hormiga's `(state, action) -> {state, effects}` is the
   **dispatcher's** shape, not reduce's — effects ride the action/holiday boundary, not
   the rewriter.
4. **expand vs materialize.** reduce's reference-expansion is **transient** (regenerated in
   the derived net each pass); `materialize` is the durable, explicit owned-state write —
   **already built in Scry**. They stay distinct; `expand` is just a rule pattern in reduce.
5. **Scheduling.** Falls out of the three-layer split: **temper eager** (per action),
   **reduce explicit + previewable** (the host calls `reduce()`; it returns a derived net
   and never mutates the source), **scry on read**.

**What was built (the faithful core).** A port-based interaction-net reducer:
`reduce/net.py` (the `Net`: `Agent`s with a principal + aux ports, a symmetric wiring map,
linearity validation, and `to_net`/`from_net` mantle adapters using a `"i:j"` port-index
edge `relation` convention — the deferred §4 port-signature groundwork) and
`reduce/reduce.py` (the `Reducer`: glyph-pair rule registration with the conflict guard,
`reduce()` with canonical + pluggable scheduling, opaque agents, the `max_steps` guard;
`annihilate`/`commute`/general `rule` constructors; `expand`; `validate`). Laws
property-tested in `reduce/reduce_test.py`: **identity, normal form, strong confluence
under randomized schedules, termination guard, locality/linearity preservation, opaque
pass-through, purity**, plus worked annihilation, commutation (duplication propagation),
and a Fountain-style `expand` example.

## Non-contradiction worth noting

Fountain feared the DAW wanted reduce to be a *live execution engine*. Reading the DAW's
actual input, it stays a **structure compiler** (reduces relationships, not audio) — so
both want structure normalization, not running the artifact. The only real difference is
*when* reduction runs (fork #5), which is scheduling, not semantics.

## Supersedes

This refines interaction-nets.md §4's "a reducer (later)" into a named three-layer plan.
The forks are now decided (above); next, fold the normative parts into `SPEC.md` and add
`reduce` / `temper` / `scry` verbs (or seams) to the dispatcher contract.
