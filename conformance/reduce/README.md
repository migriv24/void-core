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
- `commute` — for α (arity m) meeting β (arity n): the pair is replaced by
  m copies of β and n copies of α. β-copy k's principal takes α's external aux-k+1
  partner; α-copy j's principal takes β's external aux-j+1 partner; α-copy j's
  aux-k+1 is wired to β-copy k's aux-j+1 (the m×n grid). Copies duplicate the
  original's `content` (shallow) and start with **empty tags**.
- Fresh agent ids are implementation-defined — conformance compares canonical
  forms (§4), never ids.

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
 "expect": {"canonical": {...}}        // or {"error": "<kind>"}
}
```

Error kinds are abstract (each implementation maps them to its own error type):
`adapter-ports` (§4 strict adapter), `locality` (§3, strict mode only),
`termination-guard` (§3).

New cases: author `spec`/`input`, run `python run.py --regen`, **eyeball the
generated `expect` against the semantics above**, commit. The reference is the
oracle, but a golden file is only as good as its review.
