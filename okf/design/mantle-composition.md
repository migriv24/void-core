---
type: Design
title: Mantle composition — a mantle as a rune
description: A mantle with free ports is an agent of that arity, so a player mantle can be one rune in a world mantle and still have its interior reduced through by outside runes. Built at the Reduce seam 2026-08-30, with derived-agent provenance and the data-authored content rule following 2026-08-31; also settles when a boundary is a box and when it is a holiday.
resource: reduce/box.py
tags: [status:current, audience:dev, audience:library, confidence:verified, foundation]
timestamp: 2026-08-31T00:00:00Z
---

# Mantle composition — a mantle as a rune

A **player** is a mantle: body parts and clothing that interact among themselves, with
rules of their own. The same player is also a **rune** in the world mantle, next to doors
and merchants. And an amulet lying in the world, once equipped, changes the colour of a
shirt *inside* the player.

Those three sentences look like they need three mechanisms and a compromise between the
first two. They need one mechanism and no compromise, and the reason is a single line of
[interaction-net](/concepts/interaction-nets.md) theory that Void Core already adopted:

> A net with *n* free ports behaves as an **agent of arity *n***.

A mantle is a net. A net is an agent. Nothing has to be added to the model to let a mantle
be a rune — only to the **adapter**, which has to notice and splice.

## 1. What was built

`reduce/box.py`, at the [Reduce](/concepts/reduce.md) seam — no C change, no state-document
change, nothing blocked behind SPEC §12.

A **box** is a glyph. Declaring `{"boxes": {"player": {"mantle": "player", "interface":
[...]}}}` in the reduce spec means *a rune of glyph `player` **is** the player mantle*; the
adapter composes that mantle's net and splices it in at the rune's ports. Spliced agents
are namespaced `<rune>/<agent>`, so two players in one world are two independent copies —
the **rune** is the instance, not the mantle. Boxes nest, and a mantle that transitively
contains itself is refused rather than expanded forever.

The `interface` orders the sub-net's free ports; entry 0 becomes the box's principal. It
**orders the boundary and may not redefine it**: it must be a permutation of the free ports
the sub-net actually has, or composition fails. That rule is the design, not a validation
detail — see §3.

Contract: `conformance/reduce/README.md` §7, cases 17–25. Tests: `reduce/box_test.py`
(the net level) and `voidcore/box_verb_test.py` (the whole path a host uses — real
mantles, the spec as data in `config.transform`, the `reduce` verb).

## 2. Encapsulation and reach-in are the same mechanism

This is the part worth keeping, because both halves were asked for by different people and
each sounded like it cost the other.

**Void Unity asked for encapsulation** (2026-08-28): a rule inside the player must not be
able to fire on the world's silk — *"not because it was filtered, because there is no
active pair."* They were explicit that a scope **check** is the wrong shape, because a
check is something a future code path can forget to apply.

**The application asked for reach-in**: an amulet outside the player recolours a shirt
inside it.

Both hold, and neither is enforced by a rule:

- **The outside cannot reach in**, because the parent can address only the interface and
  every other inner port is already wired inside. **Linearity** — every port in at most one
  wire — does the work. There is no wire between the inner silk and the outer silk, and no
  way to add one. Nothing checks this; it is a property of the net, so it survives every
  rule set anyone writes later.
- **The outside can still affect the inside**, because a wire to interface port *k* is a
  wire to a real inner port. The amulet meets the shirt, the rule fires, and the effect
  propagates inward by ordinary reduction.

Case 20 pins the pair in one net: the amulet annihilates with the inner shirt, leaving the
amulet's chain wired to the player's own skin — an outside agent rewrote the mantle's
interior — while `voice` is untouched and *could not* have been touched, because no port of
it is in the interface.

### The finding that came out of building it

An active pair is **principal**-to-principal. So a mantle can only be *interacted with*
from outside through **inner principals that are free**. An interface made only of
auxiliary ports composes fine, wires up fine, and never reacts — a net that can be attached
to and cannot respond.

This is not a limitation to work around; it is the model telling you what an interface
means. *"What this mantle exposes for interaction"* is exactly *"which of its inner
principals are free"* — and that is decided by how the sub-mantle wires **itself**. A shirt
is equippable because its principal is unwired: the socket is the absence of a wire. Pinned
by `test_equip_socket_must_be_a_principal`.

## 3. Why the interface may only order, never redefine

An interface is a list, and the temptation is to let it *select*: expose these ports, hide
those. That would make a box a second source of truth about its own mantle, and the two
would drift — the mantle grows a rune, its free-port set changes, and the interface goes on
describing a boundary that is no longer there.

So the free ports **are** the interface. The declaration exists for one reason: the parent
addresses ports by **index**, and a set has no indices. Ordering is the whole job. A
declaration that omits a free port, names a non-free one, or repeats one is an error
(`box-interface`), and a `signatures` entry contradicting a box's computed arity is an
error rather than a tiebreak.

This is the same ruling as `validate`'s endpoint kinds (SPEC §3.7, 0.2.10) and the
`placement`-in-content de-sanctioning (0.2.5), for the same reason each time: **do not
store a fact about one entity inside another, where nothing keeps it true.**

## 4. Box or holiday? — when a boundary is which

The intuition that a **holiday** might be the mechanism here is a good one and lands one
seat over. [Interaction nets — theory](/design/interaction-nets-theory.md) §2.2 already maps
*holiday = a free port at the boundary*, so reaching for it when two nets meet is reading
the model correctly. What decides between them is **whether you reduce the far side**:

| the far side is | the boundary is | what happens at it |
|---|---|---|
| another rune, same mantle | a **wire** (`layout.edges`) | reduction fires on active pairs |
| a mantle **you own** | a **box** (§1) | the boundary *dissolves* on composition — you reduce straight through it |
| a net you **don't** own | a **holiday** (SPEC §10, the effect seam) | the port stays free; you exchange along it and never rewrite through it |

All three are the same shape — a wire to a port. The only difference is what is at the far
end and whether the executor is allowed to rewrite it.

Player and world are **both yours**, so that boundary is a box, not a holiday. Reach for a
holiday when the other side is a live server, a filesystem, another player's machine — a
net that has to be *asked*, whose rules are not yours, whose reduction you cannot perform.
Using a holiday for the player would mean routing an equip through the effect seam: I/O,
non-pure, non-previewable, outside `reduce`'s guarantees, for a rewrite that is pure graph
surgery.

Stated as a rule: **a box is composition, a holiday is communication.** If the far side
reduces when you reduce, it is a box.

## 5. Provenance, and the content rule (0.2.12)

Void Unity ported §7 on 2026-08-29 — 20/20 on first contact, *"close to a transcription"* —
and came back with two asks that this section used to list as gaps. Both are answered, and
the useful part is that they are **not the same ask**, which is what they separated for us.

### 5.1 A rule-created agent inherits its parents' box

Their framing: a host's job with a normal form is to **draw** it, which means answering
*"which GameObject does this agent belong to"* for every agent in it. A survivor answers
with its own `<rune>/` prefix; a derived agent had no answer, and the only evidence was that
it happened to be *wired* to something that still carried one — recoverable by chasing the
graph, and not recoverable at all for an agent a rule leaves free-floating.

So a rule-created agent is now prefixed with the **longest common box path of its two
parents**. `p1/silk` × `p1/wand` gives `p1/_r<hex>`; `guy/h/finger` × `guy/torso` gives
`guy/`. Parents sharing no path get **no** prefix.

Three things make this a small change rather than a scary one:

- **Decidable, not heuristic.** The minter key already contains both parent ids, so the
  owner is computable at the moment of minting from what the rewriter is already holding —
  their observation, and the reason the ask was cheap.
- **The ambiguous case stays ambiguous.** Parents from two different boxes produce no
  prefix, because such an agent genuinely has no owner. Their preference, and ours: *a
  message implying a resolution that does not exist is worse than one that says nothing* —
  the same ruling as `validate`'s fourth answer.
- **Zero blast radius, one release after freezing the digest.** In a flat net no id contains
  `/`, so the prefix is always empty and every derived id is byte-for-byte what 0.2.10
  produced. Only composed nets — newer than both existing implementations of the minter —
  can gain one. The **digest** is untouched; the prefix names the agent and never enters the
  key. This is also why `/` is now **reserved** in a reduction agent id and refused at the
  adapter: a rule that reads structure out of an id must be a fact, not a guess.

### 5.2 `patch` — content as data

Their measurement, which is better than an argument:

| half of the example | where a designer finds it |
|---|---|
| the player is a mantle **and** one rune in the world | **data** — a `boxes` entry |
| the amulet meets the shirt, and nothing reaches the voice | **data** — a rule set, plus linearity |
| the shirt comes out **purple** | **C#, behind a recompile** |

*"A game's rule set is mostly content changes with a little structure, and today the
structure is data and the content is code."* The third row is the one changed on a Tuesday
afternoon and it was the one behind a build.

`patch` is the third data rule: one pair, one survivor named by `keep`, `copy` reading
fields off the consumed agent and `set` writing literals. Deliberately weaker than an
arbitrary rewrite — close to the shape they said they reach for — so the
one-rule-per-unordered-pair confluence guard still buys what it buys.

Three properties are the whole of its contract:

- **Content-only.** Same id, glyph, arity, tags, aux wiring; only content differs.
- **It creates no active pair.** The survivor's principal is freed and nothing rewires to
  it, so `patch` cannot make a terminating rule set non-terminating, and a second dye
  cannot queue on the same garment.
- **Distinct glyphs required**, refused at compile time. On an unordered same-glyph pair
  "which side survives" has no answer that is not an arbitrary tiebreak, and an arbitrary
  tiebreak on an unordered pair is the schedule-dependence the derived-id rule exists to
  remove.

**It keeps the survivor's id**, which is a deliberate exception to the derived-id rule and
the place §5.1 and §5.2 meet: the rule governs an agent a rule *creates*, and `patch`
creates none — it hands one back. So a shirt stays `p1/shirt` through being dyed, and the
most common case in a game costs no provenance at all. Case 25 is the entire worked
scenario — player boxed into a world, amulet equipped, shirt purple — **as data, with no
code**.

## 6. What is still not done

- **`from_net` does not re-box a normal form**, and this is now **declined rather than
  pending**: Void Unity asked us not to build it, on our own reasoning back at us — after a
  rewrite spanning a boundary there is often no fact of the matter, and a host handed a
  confident wrong answer is worse off than one handed none.
- **A rule that must *restructure* on contact** — spawn agents, rewire beyond the redex
  conditionally — is still a code-registered `expand`. `patch` closed the content gap for
  the shape it covers and no more.
- **`layout.edges` still cannot name a mantle.** Nothing here needed it, which is the
  interesting part: the answer to Rung 5 was in the adapter, not the link graph. SPEC §3.7's
  rune↔mantle extension stays planned and stays blocked on §12's undoable-slice question —
  and stays no one's blocker, because a box is a fact about a rule set rather than a
  reference stored in the document.

## Status

`current` (verified 2026-08-31): composition, interfaces, instancing, nesting,
cycle/arity/interface refusals, encapsulation-by-linearity, cross-boundary reduction, the
derived-agent owner prefix, the reserved `/`, and the `patch` content rule — conformance
cases 17–25, `reduce/box_test.py` (13 groups), `voidcore/box_verb_test.py`.
Foundation: [interaction nets](/concepts/interaction-nets.md),
[reduce](/concepts/reduce.md), [mantle](/concepts/mantle.md), [links](/concepts/links.md).
