# The portable Reduce executor contract

**What this is.** The Reduce layer (the interaction-net executor, SPEC §7 [seam])
lives at the Python seam (`reduce/`), deliberately outside the C core. A host in
another language that needs Reduce — a native app compiling its own patch graphs —
therefore has to implement the executor itself. This directory is the **contract**
that makes that safe: a language-neutral statement of the executor's semantics
(§1–§5 below) plus pure-JSON conformance cases (`cases/*.json`) any implementation
can run. An implementation that passes all cases reduces every conforming net the
way the reference does.

The Python implementation (`reduce/net.py`, `reduce/reduce.py`) remains the
**reference**: where this document is ambiguous, the reference decides, and the fix
is a new case pinning the answer. If a C-ABI reduce lands in the core later, these
same cases test it — the contract outlives the seam it currently patches over.

Run the reference against the cases:

    python conformance/reduce/run.py

Port `run.py` (~100 lines) to your language to test your implementation against the
same `cases/*.json`.

## 1. The net model

- An **agent** has an `id`, a `glyph` (its type), an opaque `content` object the
  executor MUST NOT interpret, a `tags` list it MUST carry verbatim, and
  `arity` auxiliary ports. Port **0 is the principal**; ports `1..arity` are
  auxiliary. Arity comes from the **signatures** map (`glyph -> aux-port count`);
  a glyph absent from the map has arity 0.
- A **wire** connects exactly two ports, symmetrically. **Linearity**: every port
  is in at most one wire. A port in no wire is **free** (the net's boundary).
- An **active pair** (redex) is a wire joining two *principal* ports whose glyph
  pair has a registered rule and neither agent is opaque. A self-loop
  (an agent's principal wired to its own port) is never active.

## 2. The rule set (data form)

Rules arrive as data, in the `config.transform.reduce` form of the state document:

```jsonc
{"signatures": {"con": 2, "dup": 2, "era": 0},
 "rules": [{"glyphs": ["con", "con"], "rule": "annihilate"},
           {"glyphs": ["con", "dup"], "rule": "commute"}]}
```

- At most **one rule per unordered glyph pair** — registering a duplicate is an
  error. This restriction is what makes the subset confluent by construction.
- `annihilate` — the pair vanishes; the *external* partners of matching aux ports
  are cross-linked. Two flavors, selected by the optional `"swap"` key (default
  `false`), and Lafont's calculus needs both — the asymmetry is load-bearing (it is
  what makes the combinators universal): **index-straight** (`swap: false`) wires
  A's aux-i partner to B's aux-i partner (δδ — the crossing picture);
  **index-swapped** (`swap: true`) wires A's aux-i partner to B's aux-(n+1−i)
  partner (γγ — drawn between mirrored bodies, the parallel-arcs picture).
  Reversal is an involution, so either flavor is symmetric in the pair's order.
  Requires equal arity (error otherwise). At arity 0 this is pure erasure. If a
  matched boundary is free, the surviving partner is left free.
- `patch` — the **content** rule: the pair is consumed and one side survives with
  patched content. Selected by `keep` (which must name one of the pair's glyphs), with
  `copy` reading fields off the *consumed* agent and `set` writing literals:

  ```jsonc
  {"glyphs": ["dye", "cloth"], "rule": "patch", "keep": "cloth",
   "copy": {"colour": "colour"},        // survivor field <- consumed agent's field
   "set":  {"dyed": true}}              // survivor field <- literal
  ```

  Content is the survivor's, then `copy` (a source field absent on the consumed agent is
  **skipped**, not written as null), then `set` — so a literal wins over a copied field.
  **`patch` is content-only**: the survivor comes back with the same **id**, glyph,
  arity, tags and aux wiring, and only its content differs. Its **principal is left
  free** (the thing it met is gone) and the consumed agent's aux partners are freed, so
  `patch` never creates an active pair — which is why it cannot make a terminating rule
  set non-terminating, and why a second dye cannot queue on the same garment: nothing
  rewires to a free principal.
  **The glyphs MUST differ**, and this is refused at spec-compile time. On an unordered
  same-glyph pair, "which side survives" has no answer that is not an arbitrary
  tiebreak, and an arbitrary tiebreak on an unordered pair is exactly the
  schedule-dependence the derived-id rule below exists to remove.
  A `patch` that writes nothing (`set` and `copy` both absent or empty) is refused too —
  a pair that meets and changes no content is `annihilate`.
  *Why it exists:* the other two rules are structural and neither writes content
  (`commute` copies it), so *"the equipped dye makes the shirt purple"* had to be a
  code-registered `expand`. That put a game's **structure** in data and its **content**
  behind a recompile — the wrong way round, since the content half is what a designer
  changes on a Tuesday afternoon. (Void Unity measured the inversion, 2026-08-29;
  case 25 is the whole worked scenario as data.)
- `commute` — for α (arity m) meeting β (arity n): the pair is replaced by
  m copies of β and n copies of α. β-copy k's principal takes α's external aux-k+1
  partner; α-copy j's principal takes β's external aux-j+1 partner; α-copy j's
  aux-k+1 is wired to β-copy k's aux-j+1 (the m×n grid). Copies duplicate the
  original's `content` (shallow) and start with **empty tags**.
- **Fresh agent ids are derived from the redex**, and this is normative **down to the
  bytes** (it was implementation-defined before 2026-07-27, and the digest was
  non-normative before 2026-08-29). An agent created by a rule MUST be named from
  *which rule fired on which two agents*, plus an ordinal within that rewrite — never
  from a running counter, a clock, or an RNG:

  ```
  key = US.join( sorted(glyph_a, glyph_b) + sorted(parent_a, parent_b) + [str(ordinal)] )
  id  = "_r" + hex( SHA-256(UTF-8 key)[0:6] )          # 12 lowercase hex digits
  ```

  where `US` is U+001F (the ASCII unit separator), `ordinal` is **1-based** and counts
  the minter calls within one rewrite, and the hex is the digest's first 6 bytes written
  in order, lowercase. The two sorted components are sorted **separately** and are not
  interchangeable — a flat sort of all four strings is a different key and a wrong
  implementation. Both components are unordered, so the id does not depend on which side
  the executor called A. A rule MUST call the minter a deterministic number of times in
  a deterministic order for given `(a, b)`.

  **Why the property is normative.** Confluence (§3) promises the same normal form only
  *up to renaming*. A peer that merges divergent state by reducing needs the same
  **bytes**: two peers picking different, equally valid, redex orders would otherwise
  produce structurally identical nets whose agents have different names — and a name
  becomes a rune's `spirit.name`, which `layout.edges` references and tag expressions
  match, so the divergence is real rather than cosmetic. (Void Palabra's ask,
  2026-07-27.) Case 15 pins it; an implementation that mints ids sequentially passes
  every other case in this suite and fails that one.

  **Why the digest is normative too, as of 2026-08-29.** This paragraph used to end by
  saying the *hash* was not normative — the reference hashed with BLAKE2b-48 — on the
  reasoning that the cases compare id-blind canonical forms anyway. Void Unity built the
  second implementation (C#, 2026-08-28) and showed that sentence and the justification
  above it are jointly unsatisfiable: it holds the property (all sixteen of case 15's
  randomized schedules agree on the literal ids) and still cannot produce the same
  **bytes** as a peer, because it hashes with SHA-256 and the reference hashed with
  BLAKE2b. Two individually-conforming implementations then reach normal forms equal
  only up to renaming — exactly the situation this requirement exists to prevent, and
  one that raises no error anywhere: a join sees *different runes*, add-wins keeps both,
  and every rule-created agent doubles. The realistic deployment is not two copies of
  one program; it is a game client in one language and an authoring tool, server, or CI
  job in another over one document. So the digest is now named.

  **Why SHA-256 and not BLAKE2b.** Availability, and nothing else. Every standard
  library already has SHA-256 — .NET, Unity's Mono, Go, Rust, Java, Node, browsers,
  Python — so a second implementer vendors no cryptographic primitive in order to be
  conformant, and does not end up testing a reimplemented BLAKE2b against RFC 7693's
  vectors instead of Void Core's. Nothing here is a security boundary: 48 bits is a
  naming digest, and collision resistance is not the property being bought. The
  reference changed to match; `15-derived-ids.json`'s pinned ids were regenerated and
  nothing else in the suite moved, which is the blast radius the id-blind canonical form
  was designed to give.

  **A derived agent inherits its parents' box, when they share one.** In a composed net
  (§7) an agent's id may carry a `<rune>/` namespace, and a rule-created agent is prefixed
  with the **longest common box path of its two parents** — `p1/silk` and `p1/wand` produce
  `p1/_r<hex>`; `guy/h/finger` and `guy/torso` produce `guy/_r<hex>`. Parents that share no
  path (different boxes, or one unboxed) produce **no** prefix. That is a decision rather
  than a gap: such an agent genuinely has no unambiguous owner, and reporting ambiguity by
  saying nothing beats resolving it by guessing. Cases 23 and 24 pin the two halves.
  The rule is decidable, not heuristic — the rewriter is already holding both parent ids
  at the moment it mints — and it exists because a host's job with a normal form is usually
  to **draw** it, which means answering "which entity does this agent belong to" for every
  agent in it (Void Unity, 2026-08-29). **The digest is untouched**: the prefix names the
  agent and never enters the key. And in a **flat** net no id contains `/`, so the prefix is
  always empty and every id is byte-for-byte what it was before composition existed — which
  is how this could ship one release after the digest was frozen.

  **`patch` is the one exception, and it creates nothing.** A `patch` rule hands back an
  existing agent with new content rather than minting one, so the survivor keeps its own
  id — including its `<rune>/` provenance. This paragraph governs agents a rule *creates*;
  there is no conflict, and it is the honest reading: same glyph, same arity, same wiring,
  same tags, so it is the same agent. It stays schedule-independent because it is an
  **input** id.

  **Case 16 pins the minter alone** — key in, id out, with no reduction around it — so
  an implementer can debug the hash without debugging the rewriter. Its vectors separate
  the parts that fail independently: that each component is sorted, that the two
  components are *not* interchangeable with each other, that the ordinal is 1-based and
  part of the key, and the `_r<12 hex>` rendering. Get case 16 green first; case 15 then
  tests the thing it is for, which is the ids' independence from the schedule.

## 3. Reduction

- `reduce(net) -> net` is **pure**: the input net is never mutated; no effects,
  no clock, no I/O. The result is a new net at **normal form** (no active pairs).
- **Locality**: a rule only rewires the redex's own boundary. A matched aux port's
  partner may itself be *inside* the redex (the pair wired aux-to-aux, or an
  agent's aux wired to its own other aux, as well as principal-to-principal) —
  that is a completely legal net, and by default the executor MUST resolve it
  by chasing the wire equations through the redex (union-find over the redex's
  aux slots): a chain with **two external ends** becomes one bridging wire, a
  chain with **one external end** stays free, and a **closed chain** (a loop
  with no agents on it) **vanishes** — the net model cannot represent an
  agentless wire, so loops are dropped, not tracked; a host that wants loops
  as values must count them itself, outside this contract. Under `commute`
  this falls out of the same equations: an internal aux wire ends up joining
  the corresponding copies' *principals* — a fresh active pair, which then
  reduces normally (Lafont's own picture).
  A case (or host) may instead demand the **restricted subset** with
  `"strict_locality": true`: the executor MUST then fail with the `locality`
  error rather than construct any wire through the redex. This mode is the
  original contract behavior, kept for hosts that want "no internal wiring"
  as a structural guarantee; case 09 pins it.
- **Scheduling**: any order of redex selection MUST reach the same normal form
  (strong confluence holds on this subset by construction). The reference's
  default schedule is "canonical first" — active pairs as sorted `(id, id)`
  tuples, lowest first — but conformance never depends on it: expected values are
  canonical forms, and cases with `"schedules": N` direct the runner to verify N
  randomized schedules converge.
- **Opaque agents**: the `opaque` set (glyph names or agent ids) freezes agents —
  a pair involving one is not active.
- **Termination**: rule sets are Turing-complete, so the executor takes a
  `max_steps` guard (reference default 100000) and MUST fail with the
  `termination-guard` error when exceeded — never loop unbounded.

## 4. The mantle adapter and the canonical form

- **`/` is reserved in an agent id** and the adapter refuses a rune whose name contains
  one (`adapter-ports`, case 22). A spliced agent is named `<rune>/<agent>` (§7), and both
  the box path and the derived-id owner prefix (§2) read structure back out of an id — so
  a rune literally named `a/b` in a **flat** mantle would otherwise read as an agent
  belonging to a box called `a`. Refusing the input is the standing preference over
  guessing at it. The reservation holds in every mantle, not only composed ones.
- `to_net(mantle, signatures)`: each rune becomes an agent (`id` = spirit name,
  `glyph`, `content`, `tags`); each `layout.edges` entry becomes a wire, reading
  port indices from `relation` as `"i:j"` (from-port i ↔ to-port j). An edge whose
  relation is not `"i:j"` MUST be rejected (`adapter-ports` error) — port order is
  app knowledge and the adapter never guesses. `from_net` is the inverse
  projection; tags ride back onto the runes.
- Conformance compares **canonical forms** — id-independent JSON fingerprints:

```jsonc
{"agents": [ [glyph, content, sorted-tags], ... ],   // sorted
 "wires":  [ [[glyph, port], [glyph, port]], ... ],  // each wire once, endpoints sorted, list sorted
 "free":   [ [glyph, port], ... ]}                   // sorted
```

  All sorting is by the canonical JSON text of the element (keys sorted, no
  whitespace) — see `canonical()` in `run.py`. Note this fingerprint identifies
  agents by glyph+content+tags, not by wiring context: it is a normal-form
  comparator for the discriminating cases in this suite, not a general
  graph-isomorphism check. Author case content accordingly (give look-alike
  agents distinguishing content — and where the *wiring* is what the case pins,
  distinguishing **glyphs**, since wire endpoints carry only `[glyph, port]`).

## 5. Case format and error kinds

```jsonc
{"case": "NN-name",
 "description": "what this pins",
 "spec": { signatures, rules },        // §2 data form (rules may carry "swap")
 "input": { runes, layout.edges },     // a mantle fragment (§4)
 "opaque": ["glyph-or-id"],            // optional
 "max_steps": 100000,                  // optional
 "schedules": 8,                       // optional: verify N randomized schedules
 "strict_locality": true,              // optional: demand the restricted subset (§3)
 "pin_ids": true,                      // optional: also compare the literal agent ids (§6)
 "expect": {"canonical": {...}}        // or {"error": "<kind>"}
}
```

Error kinds are abstract (each implementation maps them to its own error type):
`adapter-ports` (§4 strict adapter), `locality` (§3, strict mode only),
`termination-guard` (§3).

A case may also carry **`mantles`** — a list of whole mantles the case's `input` can box
(§7). A rune whose glyph is declared in `spec.boxes` is spliced in as the named mantle's
net; with no `boxes` the key is ignored and the adapter is the flat one.

Error kinds added by §7: `box-interface` (the declared interface is not a permutation of
the sub-net's free ports), `box-mantle` (a box names a mantle the case does not supply),
`box-cycle` (a mantle transitively contains itself).

**Carry the kind in a field, not in the message.** A runner that recovers the kind by
searching a diagnostic for keywords silently reclassifies a case the moment a diagnostic is
reworded, and a case that changes kind without changing behaviour is precisely what a
conformance suite exists to prevent. Ours did that until 2026-08-30 and it had already
produced one live inconsistency: a malformed edge naming a non-existent endpoint answered
`adapter-ports` through the flat adapter and `box-interface` through the composing one,
depending only on whether some *other* rune in the mantle happened to be a box. Both are
`adapter-ports` — it is a malformed edge either way — and **case 21** now locks both paths.
(Both found by Void Unity, 2026-08-29, by porting this runner rather than reading it.)

In a minter case, **`ordinal` is 1-based**: it counts minter calls within one rewrite, so
`0` is a malformed vector rather than "no id", and a runner MUST say so rather than fall
out of its loop.

A case carrying a **`minter`** key instead of `spec`/`input` is a minter-vector case
(§2): no net, no reduction, just the id function, one vector per entry and one id per
vector in the same order.

```jsonc
{"case": "16-minter",
 "minter": [{"glyphs": ["con","dup"], "parents": ["a","b"], "ordinal": 1}, ...],
 "expect": {"ids": ["_r2506cbce0c40", ...]}}
```

New cases: author `spec`/`input`, run `python run.py --regen`, **eyeball the
generated `expect` against the semantics above**, commit. The reference is the
oracle, but a golden file is only as good as its review.

## 6. `pin_ids` — checking identity, not just shape

Most cases are id-blind by design: the canonical form (§4) identifies agents by
glyph+content+tags, so an implementation is free to name agents however it likes and
still pass. A case with `"pin_ids": true` opts into the stronger check — the runner adds
`"ids"` (the sorted agent names) to the expectation, and, when combined with
`"schedules": N`, additionally requires that all N randomized schedules produce the
**same literal ids**, not merely the same canonical form.

Use it where reproducible identity is the point (case 15) and leave it off elsewhere, so
the rest of the suite keeps testing semantics rather than a naming scheme.

The pinned `_r<hex>` strings in case 15 are **normative** as of 2026-08-29, not merely
the reference's rendering: §2 now names the digest, so an implementation that differs
there is wrong rather than differently-flavoured, and the fix is to match the digest,
not to re-`--regen` the case against itself. (That re-`--regen` advice stood here until
2026-08-29, and Void Unity named a second reason it was bad: a host that **vendors**
this directory cannot write to it, because a vendored copy edited locally stops showing
drift, which is the whole reason to vendor one. Re-deriving the expectation in memory
and reporting the difference every run — what they did instead — was the right call
under the old text.)

If your ids differ, run **case 16** first: it isolates the minter from the rewriter.

---

## 7. Composition — a mantle as a rune inside another mantle

A net with *n* free ports **is** an agent of arity *n*. That is Lafont's reading, and it is
the whole content of "a player is a mantle of body parts and clothing, and also one rune in
the world." So this needs no new primitive: the adapter notices that a rune's glyph names a
mantle and **splices** that mantle's net in at the rune's ports.

```jsonc
{"signatures": {"body": 1, "cloth": 2, "sound": 0},
 "boxes": {"player": {"mantle": "player",
                      "interface": ["skin:0", "shirt:0", "voice:0", "shirt:2"]}},
 "rules": [...]}
```

- A **box** is a glyph. A rune of that glyph *is* the named mantle; there is no other
  marker on the rune, and nothing is stored in the state document that has to be kept
  true — a box is a fact about a rule set, not about a rune.
- `interface` lists the sub-net's free ports as `"<agent>:<port>"`, in the order the
  parent addresses them: entry 0 becomes the box's **principal**, the rest its
  auxiliaries, so the box's arity is `len(interface) - 1`. When boxes nest, an outer
  interface names ports of the *composed* sub-net, spliced ids included
  (`"h/finger:0"`).
- The declaration **orders the boundary and may not redefine it**. It MUST be a
  permutation of the free ports the sub-net actually has — every free port declared,
  no others, no repeats — or the composition fails with `box-interface`. The free ports
  *are* the interface; the list exists only because the parent addresses them by index
  and a set has no indices. For the same reason a `signatures` entry that contradicts a
  box's computed arity is an error rather than a tiebreak: one fact, one source.
  `interface` may be omitted, giving canonical (sorted) order — deterministic, and
  arbitrary, so declare it whenever the parent's edges care which port is which.
- Spliced agent ids are namespaced **`<rune>/<agent>`**, and this is normative (case 17
  pins it with `pin_ids`). By the *rune*, not the mantle: the rune is the instance, so
  `p1` and `p2` of the same mantle are two independent copies rather than one shared net.
- A mantle that transitively contains itself is `box-cycle`. A mantle with no free ports
  has no interface and cannot be a rune (arity 0 leaves nothing to wire).
- **With no boxes declared, composition is exactly `to_net`** — same net, same errors.
  Every case in this suite that predates §7 runs down the identical path.

### 7.1 Why this gives encapsulation and interaction at once

They look like opposite requirements, and are the same one:

- **The outside cannot reach in.** The parent can address only the interface; every other
  inner port is already wired inside. Linearity — every port in at most one wire — does
  the work. An inner `silk` and an outer `silk` form no active pair because there is no
  wire between them and no way to add one. This is not a scope check that a future rule
  could forget to apply; it is a property of the net, so it holds under every rule set
  anyone writes later.
- **The outside can still affect the inside**, through the interface, because a wire to
  interface port *k* is a wire to a real inner port. An equipped item meets the garment it
  is wired to, the rule fires, and the effect propagates inward by ordinary reduction.
  Case 20 pins exactly this: an amulet outside the player annihilates with the shirt
  inside it, leaving the amulet's chain wired to the player's own skin — an outside agent
  rewrote the mantle's interior, using nothing but a wire to a free port.

**One consequence worth stating plainly, because it is easy to design around wrongly:** an
active pair is *principal*-to-principal, so a mantle can only be **interacted with** from
outside through inner principals that are free. An interface of nothing but aux ports
composes fine and never reacts — it is a net that can be attached to and cannot respond.
"What this mantle exposes for interaction" is exactly "which of its inner principals are
free," and that is a modelling decision the sub-mantle makes by how it wires itself.

### 7.2 Provenance, and what composition still does not do

**Every agent in a composed normal form can be attributed, or is honestly unattributable.**
Three cases, and the first two are the common ones:

| agent | its box |
|---|---|
| a **survivor** | its own `<rune>/` prefix, unchanged |
| a **`patch` survivor** | likewise — `patch` keeps the id, so a content rewrite costs no provenance |
| a **rule-created** agent | the longest common box path of its two parents (§2), or none when they share none |

That covers the case a host actually has: an effect fires *inside* one character far more
often than *between* two, and the between case has no honest answer anyway.

**What is still not done — and one of these is deliberate rather than pending:**

- **`from_net` does not re-box a normal form into sub-mantles**, and will not on request:
  after a rewrite spanning a boundary there is often no fact of the matter about which side
  a new agent belongs to, and a host handed a confident wrong answer is worse off than one
  handed none. (Void Unity, 2026-08-29, asked us *not* to build this, and separated it
  cleanly from the provenance ask above — which is why the provenance ask got built.) A
  host that wants a sub-mantle's own state back should reduce that mantle by itself, which
  is exact and cheap.
- **The rule set is still three rules.** `patch` closed the content gap for the shape it
  covers — one pair, one survivor, a content patch — and it is deliberately weaker than an
  arbitrary rewrite so the one-rule-per-unordered-pair confluence guard still means
  something. A rule that needs to *restructure* on contact (spawn agents, rewire beyond the
  redex boundary conditionally) is still a code-registered `expand`.
