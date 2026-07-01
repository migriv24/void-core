---
type: Design
title: Interaction nets — the theory
description: Lafont interaction nets as Void Core's structuring/reasoning model (runes=agents, mantle=net+rules, holiday=boundary port).
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Interaction Nets as the Foundation of Rune / Mantle / Holiday

> Source: NeLA (`github.com/heikowagner/nela-lang`, **cloned locally at
> `../../nela-lang`** for study) and the theory it builds on — Lafont's
> **interaction nets** (1990) and **interaction combinators** (1997), with
> **linear logic** (Girard 1987) for resource semantics.
>
> ⚠️ **Read [what-voidcore-is-not.md](/design/what-voidcore-is-not.md) alongside this.**
> NeLA uses interaction nets as an *execution engine* (it reduces nets to compute
> results). Void Core borrows them as a *structuring/reasoning* model only. We
> take the **shape**, not the runtime.
>
> **This is the highest-value track.** It turns the hand-wavy Codex line "a
> mantle defines the rules of interaction between its runes" into a precise,
> well-studied formalism — and it gives clean, formal answers to questions our
> own `LEARNINGS.md` left open (binding conflicts, confluence, holidays).

---

## 1. What an interaction net is (the 60-second version)

- A net is a graph of **agents** (cells). Each agent has a **type** (symbol) with
  a fixed **arity** n: it has exactly **one principal port** and **n auxiliary
  ports**.
- **Wires** connect ports to ports. Unconnected ports are **free ports** — they
  form the net's **interface** (its boundary).
- An **active pair** (a *redex*) is two agents joined **principal-port to
  principal-port**. That, and only that, is where computation can happen.
- An **interaction rule** rewrites an active pair `(α, β)` into a small net that
  reconnects the same free auxiliary ports. There is **at most one rule per
  unordered pair of agent types**.
- **Reduction** = repeatedly replace active pairs by their rule's right-hand side
  until no active pairs remain (**normal form**).

Three properties make this special:

- **Locality** — a rule only ever touches the two agents in the redex and rewires
  their ports. Nothing elsewhere in the net is consulted or changed. No global
  state.
- **Strong confluence (the "diamond")** — if a net can reduce two different ways,
  both results reduce to a common net. Consequence: **the normal form is unique
  and the number of reduction steps is fixed, regardless of order.** Determinism
  without sequencing.
- **Linearity** — each rewrite *consumes* its redex; nothing is implicitly copied
  or dropped. Duplication and erasure must be done by *explicit* agents (the
  duplicator δ / `DUP` and eraser ε / `ERA`). Interaction combinators show **three
  agents (γ, δ, ε) are Turing-complete.**

NeLA's pragmatic move is a **two layer** design: `NELA-S`, a Haskell-like surface
syntax that is easy for an LLM to write, compiles to `NELA-C`, an explicit
interaction-net bytecode that reduces deterministically. **Surface for humans/LLMs;
net for semantics.** Keep that split in mind — Void Core wants the same shape.

### 1.1 Grounding: the actual rules (from the cloned source)
The README only gestures at rules; the real ones live in `nelac_runtime.c`'s
`fire()` and `nela_compiler.py`'s 25-agent vocabulary (`CON γ`, `DUP δ`, `ERA ε`,
`APP`, `LAM`, `PAR`, arithmetic/compare/bool ops, `MAT`, `FREF`, …). The shape of
a rule, confirmed in code:

- **`APP ⊳ LAM` (β-reduction)** — the canonical *annihilation*: kill both agents,
  then `link(result, body)` and `link(arg, var)`. Two agents vanish, four ports
  rewire. Purely local.
- **`DUP ⊳ LAM` / `DUP ⊳ CON` (commutation)** — copying a structured agent spawns
  *two* copies of it plus *new DUPs* pushed down onto each of its ports. This is
  how duplication propagates lazily through a net.
- **`DUP ⊳ atom`** — a leaf (`INT/FLT/STR/BOO/NIL/…`) is copied by allocating two
  leaves; `ERA` discards. Duplication and erasure are **explicit agents**, never
  implicit — that *is* linearity in mechanism.
- **Trigger discipline:** only **principal ports (`port[0]`) fire**; auxiliary
  ports are pure data flow. Every rule is `kill the pair + relink ports` —
  nothing else in the net is touched (locality), and the reducer is a sequential
  work-queue that is *provably safe to parallelize* by strong confluence.

The takeaway for us is the **rule *form***, not the arithmetic: *an interaction is
`(agentA, agentB)` meeting at principals → a small local rewrite of their ports.*
That is the template for a mantle's rule table (§2.1).

---

## 2. The mapping to Void Core

This is the heart of it. The correspondence is remarkably clean:

| Interaction net | Void Core | Notes |
|---|---|---|
| **Agent / cell** | **Rune** | the atomic unit that participates in interaction |
| Agent **type / symbol** (fixes arity) | **Glyph** | the glyph should carry a **port signature** (arity + named ports) |
| **Principal port** | a rune's primary interaction handle | how it "meets" another rune; tied to its `spirit` |
| **Auxiliary ports** | a rune's connection slots | where its relations / content anchors wire in |
| **Wire** | a **relation** (`layout.edges`) or **binding** | today untyped `{from,to,relation}`; nets type them port-to-port |
| **The net + its interaction-rule table** | **Mantle** | ← this is the precise meaning of "a mantle controls the rewrite rules of its runes" |
| **Free / boundary port** (open wire) | **Holiday** | the place a net wires out to *another* net it does not own |
| **Net composition** (wire two boundaries) | **Binding across mantles** | already in `LEARNINGS.md`; nets formalize it |
| **Reduction** (firing rules) | the mantle **"running"** / resolving | future: a reducer; now: stored, not executed |

### 2.1 "A mantle controls the rewrite rules of its runes" — decoded
A mantle is not just a bag of runes. It carries a **rule table** keyed by glyph
pairs: *"when a rune of glyph A meets a rune of glyph B at their principal ports,
replace them with this sub-net."* That sentence is now a data structure, not a
vibe. The Codex's "rules of interaction, ordering, and display" splits cleanly:
- **behavioral rules** → interaction rules (this doc),
- **layout/display** → a separate constraint system (the LEARNINGS note already
  reached this; unchanged here).

### 2.2 Holiday = boundary port (this is the big unlock)
Your own framing — *"a holiday is a broadcast that calls for another domain… it's
things we don't control… if we DO control it we place a mantle on it"* — **is
exactly the free-port/boundary concept.** A mantle-net you control reduces
internally; a **holiday is a free port at its boundary** that wires into another
net (ImgBB, Supabase, the Antfarm) which you do *not* reduce — you just exchange
along the wire. Composing a controlled net with an uncontrolled one is **net
composition through boundary ports.** The Antfarm node and the holiday are the
same object seen from inside (`VOIDCORE_INTEGRATION.md` in Hormiga) and from the
theory (here).

---

## 3. Formal answers to our open questions

`LEARNINGS.md` worried: *if two of Click's bindings fire from the same event, does
order matter? Do we need Petri nets with inhibitor arcs, or priorities?* Interaction
nets answer this directly:

- **Order doesn't matter — by construction.** If interactions are expressed as
  proper interaction rules (≤ one rule per agent-pair, acting locally), reduction
  is **strongly confluent**. No priority policy, no inhibitor arcs needed for a
  well-formed rule set. The Petri-net machinery we were reaching for is heavier
  than the problem.
- **The real constraint ("one mouth, can't speak twice at once") is a *resource*,
  not an ordering problem.** Model the speech-slot as a **linear resource** (a
  token consumed exactly once). Linearity then *guarantees* a single coherent
  outcome without us hand-coding precedence. This is cleaner and more principled
  than the "priorities now, Petri nets later" plan.
- **Your "rune ≈ monoid, mantle ≈ Petri net + graph rewrite" intuition, revised:**
  interaction nets are the better frame. A rune is an **agent**, not a monoid
  (runes don't obviously have an associative compose-with-identity). A mantle is an
  **interaction net** (agents + wires + rule table), which subsumes the useful
  parts of "Petri net + graph rewrite" *and* adds confluence + locality. Keep
  Petri nets only as loose intuition for token/event flow.

---

## 4. What we adopt now vs. later (be careful here)

The danger: if runes stay **untyped content-bags forever**, we foreclose this
entire foundation. The discipline (same as the Codex's layout/rules decision):
**add the structure as metadata now, build the executor later.**

**Now (cheap, additive, doesn't break Biology):**
- Give a glyph an optional **port signature**: arity + named ports + which port is
  principal. Stored on the glyph definition; unused by today's code.
- Let relations / bindings optionally name the **ports** they connect, not just
  the runes. (`{from: rune.port, to: rune.port}`.)
- Let a mantle optionally carry an **interaction-rule table** (glyph-pair →
  rewrite). Stored as data on the mantle (`rules` already exists as a reserved
  field!), validated, *not executed*.
- Adopt **linearity as documentation**: mark which runes/resources are
  consume-once. No enforcement yet.

**Later (research / "our own compiler"):**
- A **reducer** that finds active pairs and fires rules; confluence checking;
  linearity enforcement; an explicit net representation (à la NELA-C).
- Possibly a **two-layer split** mirroring NeLA: Voidscript (surface) compiles to
  a net (semantics). See [dsl-and-pel.md](/design/voidscript-dsl.md) — PEL's homoiconic AST
  and this net substrate are two halves of the same idea.

**Don't:**
- Don't build the reducer to ship a working core. Don't compile Hormiga
  newsletters to net bytecode. Premature. The model must merely *be expressible*
  as a net, so we never re-model.

---

## 5. Why this matters for the long game
A net is a small, regular, compositional, **local** structure with a fixed agent
vocabulary. That is *exactly* the kind of substrate that (a) an LLM can reason
about reliably (NeLA's whole pitch), and (b) maps cleanly to a learned embedding
space later (see [context-and-rl.md](/design/context-optimization.md), §far-future). Choosing
the interaction-net frame now is what keeps the "commands as high-dimensional
vectors" north star reachable instead of fantastical.

> Open: NeLA's repo documents the agent set (CON/DUP/ERA/PAR/INT/… 25 agents) and
> the two-layer compile flow, but **not** the concrete interaction-rule tables.
> Before committing, we should read Lafont's combinator rules directly (γδε
> commutation/annihilation) to design our glyph-pair rule schema honestly.
