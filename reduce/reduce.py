"""
reduce/reduce.py — the Reduce layer: the interaction-net executor (graph rewriter).

Reduce fires a [mantle](okf/concepts/mantle.md)'s interaction rules on active pairs until
no rule applies (**normal form**), producing a **derived** net — the source is never
touched. It is the executor [interaction nets](okf/concepts/interaction-nets.md) reserved.
Design + fork resolutions: `notes/reducer.md`.

The shape (resolved forks):
- **Restricted confluent subset** (fork 2): at most one rule per *unordered glyph pair*,
  principal-to-principal, local. Strong confluence holds **by construction** there — the
  normal form is unique regardless of reduction order. Uniqueness is enforced at
  registration (a duplicate pair raises).
- **`reduce(net) -> net`** (fork 3): pure; emits no effects (those live at the holiday
  boundary). Functional — returns a derived net, never mutates the source (fork 5:
  explicit + previewable).
- **Active pairs only reduce** (fork 1): a wire joining two principal ports whose glyphs
  have a rule. Everything else is inert, so feedback cycles are preserved for free; the
  host may also mark agents **opaque** (by glyph or id). Termination is not guaranteed in
  general (interaction combinators are Turing-complete), so a `max_steps` guard raises
  `ReduceError`.

- **Rule-created agents are named from the redex** (not from a counter), so reduction is
  reproducible across *peers*: two reducers that pick different — equally valid — redex
  orders produce byte-identical ids, not merely nets equal up to renaming. See
  `fresh_id_minter`, whose digest is **normative** down to the bytes — the peers that
  have to agree are not two copies of this file. (Void Palabra's ask, 2026-07-27:
  merge-by-reduction needs the normal form to be the same *bytes*, not the same shape.
  Void Unity, 2026-08-28: and the second implementation is in another language.)

A **rule** is `fn(a, b, fresh) -> Rewrite`: given the two redex agents (in the order the
rule was registered) and a fresh-id minter, return new agents + new wires. A rule MUST
call `fresh()` a deterministic number of times in a deterministic order for given
`(a, b)` — that is what keeps the derived ids stable. Wires address
the redex's freed auxiliary ports symbolically with `A(i)` / `B(i)` (the *external*
partner of a's / b's aux port i), or a new agent's port `(id, idx)` directly. This is the
locality discipline: a rule only rewires the redex's own ports.

An aux port's partner may itself be *inside* the redex (the pair also wired aux-to-aux,
or an agent's aux wired to its own other aux) — a legal net in full Lafont semantics. By
default the executor resolves these **internal redex wires** by chasing the wire
equations through the redex (union-find): a chain with two external ends becomes one
bridging wire, one external end stays free, and a closed chain (a loop with no agents on
it) vanishes — the net model cannot represent an agentless wire, so loops are dropped,
not tracked. `reduce(..., strict_locality=True)` restores the restricted subset's
rejection (raises with the locality error) for hosts that want the old guarantee.
"""
from __future__ import annotations

import hashlib
from dataclasses import dataclass, field
from typing import Callable, Optional

from net import SEP, Agent, Net, NetError, Port, box_path


class ReduceError(RuntimeError):
    """Reduction exceeded `max_steps` (a non-terminating rule set) or a rule was malformed."""


# ── symbolic references to the redex's freed external ports ───────────────────────
@dataclass(frozen=True)
class Ext:
    side: str   # 'A' or 'B'
    port: int   # aux index 1..arity


def A(i: int) -> Ext:
    """The external partner of redex-agent A's aux port i (use in a Rewrite link)."""
    return Ext("A", i)


def B(i: int) -> Ext:
    """The external partner of redex-agent B's aux port i."""
    return Ext("B", i)


PortRef = "Ext | Port"  # an Ext, or a concrete (agent_id, idx) for a new agent


@dataclass
class Rewrite:
    """The right-hand side of an interaction rule: agents to introduce and the wires that
    reconnect the redex's freed ports. Linearity is checked when applied."""
    new_agents: list[Agent] = field(default_factory=list)
    links: list[tuple] = field(default_factory=list)   # (PortRef, PortRef)


Rule = Callable[[Agent, Agent, Callable[[], str]], Rewrite]


# ── identity of rule-created agents: derived from the redex, never from the order ──
def fresh_id_minter(ga: str, gb: str, a_id: str, b_id: str) -> Callable[[], str]:
    """Build the id minter for one rewrite. Ids are derived from **which rule fired on
    which two agents**, plus an ordinal within that rewrite — never from a running counter.

    This is what makes reduction reproducible across *peers*. Confluence promises two
    reducers the same normal form **up to renaming**; a sequential counter cashes that
    promise out as `_r1`, which names "the first agent the first firing happened to
    create" — a fact about the schedule, not about the net. Two peers picking different
    (equally valid) redex orders then produce structurally identical nets whose agents
    have different names, and since a name is a rune's `spirit.name` — the thing
    `layout.edges` references and tag expressions match — the divergence is real and not
    cosmetic. Deriving from the redex removes the schedule from the equation: by
    induction from the input agents (identical on both peers), every derived id is too.

    The pair is unordered in both components, so the id does not depend on which side the
    executor happened to call A. The ordinal makes a rule's own copies distinct and is
    deterministic because a rule calls `fresh()` a fixed number of times, in a fixed
    order, for given (a, b) — that is a requirement on rules, met by everything below.
    Purity is unaffected: hashing reads no clock and no RNG.

    **The digest is normative** (conformance/reduce/README.md §2), not merely the
    property: SHA-256 over the UTF-8 of `\x1f`-joined
    `sorted(glyph_a, glyph_b) + sorted(parent_a, parent_b) + str(ordinal)`, truncated to
    the first 6 bytes, rendered `_r` + 12 lowercase hex. It reads as an internal choice
    and is not one — the peers that must agree on these bytes are a C# game client and a
    Python authoring tool over one document, so a digest only *this* file can compute
    hands them structurally identical nets whose agents have different `spirit.name`s,
    which a join then keeps twice. SHA-256 rather than the BLAKE2b-48 used until
    2026-08-29 for exactly one reason: every standard library already has it, so the
    second implementer vendors no cryptographic primitive in order to be conformant
    (Void Unity, 2026-08-28, from .NET/Mono, which has no BLAKE2b). Nothing here is a
    security boundary — 48 bits is a naming digest, and collision resistance is not the
    property being bought."""
    key = "\x1f".join(sorted((ga, gb)) + sorted((a_id, b_id)))
    owner = _owner_prefix(a_id, b_id)
    n = [0]

    def fresh() -> str:
        n[0] += 1
        digest = hashlib.sha256(f"{key}\x1f{n[0]}".encode("utf-8")).hexdigest()[:12]
        return f"{owner}_r{digest}"
    return fresh


def _owner_prefix(a_id: str, b_id: str) -> str:
    """The box a derived agent belongs to, as an id prefix — the **longest common box
    path** of its two parents, or `""` when they share none.

    A host's job with a normal form is usually to *draw* it, which means answering "which
    entity does this agent belong to" for every agent. A surviving agent answers with its
    own `<rune>/` prefix; a derived one had no answer at all, and the only evidence was
    that it happened to be wired to something that still carried one — recoverable by
    chasing the graph, and not recoverable for an agent a rule leaves free-floating.
    (Void Unity, 2026-08-29, who asked for exactly this and explicitly did **not** ask for
    `from_net` to re-partition a normal form.)

    The rule is decidable rather than heuristic, which is why it is worth having: both
    parents inside `p1` means the child is unambiguously `p1`'s, and the rewriter is
    already holding both ids at the moment it mints. Parents from **different** boxes share
    no path and get **no** prefix — reported as ambiguous by saying nothing, the same
    choice `validate` makes about an endpoint that resolves to a rune in another mantle: a
    message implying a resolution that does not exist is worse than one that says nothing.

    Compatibility, and the reason this could ship one release after the digest was frozen:
    in a **flat** net no id contains `SEP`, so both paths are empty, the prefix is `""`,
    and every id is byte-for-byte what 0.2.10 produced. Only composed nets — which are
    newer than both existing implementations of the minter — can gain a prefix. The
    **digest** is untouched: the prefix names the agent, it does not enter the key."""
    common: list[str] = []
    for x, y in zip(box_path(a_id), box_path(b_id)):
        if x != y:
            break
        common.append(x)
    return "".join(p + SEP for p in common)


# ── the reducer ───────────────────────────────────────────────────────────────────
@dataclass
class Reducer:
    # key: frozenset({ga, gb}) -> (ga, gb registered order, fn). One rule per pair.
    rules: dict = field(default_factory=dict)

    def rule(self, ga: str, gb: str, fn: Rule) -> "Reducer":
        """Register the (single) rule for the unordered glyph pair {ga, gb}. Raises on a
        duplicate — this *is* the confluence conflict guard (fork 2)."""
        key = frozenset((ga, gb))
        if key in self.rules:
            raise ValueError(f"a rule for the pair {set(key)} is already registered "
                             f"(at most one rule per glyph pair — confluence guard)")
        self.rules[key] = (ga, gb, fn)
        return self

    # ── active pairs ───────────────────────────────────────────────────────────────
    def active_pairs(self, net: Net, opaque: set[str]) -> list[tuple[str, str]]:
        """Unordered (a_id, b_id) pairs joined principal-to-principal with a rule, neither
        opaque. Sorted for a canonical default schedule."""
        seen = set()
        out = []
        for a in net.agents.values():
            if a.id in opaque or a.glyph in opaque:
                continue
            q = net.partner((a.id, 0))
            if q is None or q[1] != 0 or q[0] == a.id:
                continue
            b = net.agents[q[0]]
            if b.id in opaque or b.glyph in opaque:
                continue
            if frozenset((a.glyph, b.glyph)) not in self.rules:
                continue
            key = tuple(sorted((a.id, b.id)))
            if key not in seen:
                seen.add(key)
                out.append(key)
        return sorted(out)

    # ── one rewrite step ─────────────────────────────────────────────────────────
    def _fire(self, net: Net, a_id: str, b_id: str,
              strict_locality: bool = False) -> None:
        a, b = net.agents[a_id], net.agents[b_id]
        ga, gb, fn = self.rules[frozenset((a.glyph, b.glyph))]
        # present agents to the rule in its registered order
        if a.glyph != ga or b.glyph != gb:
            a, b = b, a
        fresh = fresh_id_minter(ga, gb, a.id, b.id)
        # snapshot external partners of the redex's aux ports *before* deletion
        ext = {
            "A": {i: net.partner((a.id, i)) for i in range(1, a.arity + 1)},
            "B": {i: net.partner((b.id, i)) for i in range(1, b.arity + 1)},
        }
        side_of = {a.id: "A", b.id: "B"}
        rw = fn(a, b, fresh)

        if strict_locality:
            self._apply_strict(net, a, b, ga, gb, ext, side_of, rw)
        else:
            self._apply_resolving(net, a, b, ga, gb, ext, side_of, rw)

    def _apply_strict(self, net, a, b, ga, gb, ext, side_of, rw) -> None:
        """The restricted subset: any wire touching an internal redex port is an error."""
        def resolve(ref):
            if isinstance(ref, Ext):
                p = ext[ref.side].get(ref.port)
                if p is not None and p[0] in side_of:
                    raise ReduceError(
                        f"rule for {set((ga, gb))} referenced {ref.side}({ref.port}), "
                        f"which is an internal redex wire ({p}) — rules may only rewire "
                        f"ports leading *out* of the redex (locality).")
                return p  # may be None == that boundary was free
            return ref  # a concrete (id, idx) on a new agent

        net.remove_agent(a.id)
        net.remove_agent(b.id)
        for ag in rw.new_agents:
            net.add(ag)
        for r1, r2 in rw.links:
            p, q = resolve(r1), resolve(r2)
            if p is None or q is None:
                continue  # a freed boundary stays free
            net.connect(p, q)

    def _apply_resolving(self, net, a, b, ga, gb, ext, side_of, rw) -> None:
        """Full Lafont semantics: internal redex wires are resolved by chasing the wire
        equations through the redex. Union-find over connection points — each redex aux
        *slot* plus every hard end (an external port or a new agent's port). A rule link
        joins its two ends; a snapshot wire joins its slot to its partner (another slot
        when the wire is internal). Each resulting chain with two hard ends becomes one
        wire; one hard end stays free; a closed chain (loop) has nothing to attach and
        vanishes. Under commutation this wires the corresponding copies' principals
        together — a fresh active pair, Lafont's own picture."""
        parent: dict = {}

        def find(x):
            parent.setdefault(x, x)
            root = x
            while parent[root] != root:
                root = parent[root]
            while parent[x] != root:            # path compression
                parent[x], x = root, parent[x]
            return root

        def union(x, y):
            parent[find(x)] = find(y)

        def point(ref):
            if isinstance(ref, Ext):
                return ("slot", ref.side, ref.port)
            return ("port", ref)

        for r1, r2 in rw.links:                  # the rule's wire equations
            union(point(r1), point(r2))
        for side, snap in ext.items():           # the pre-existing wires at the boundary
            for i, p in snap.items():
                if p is None:
                    continue                     # free boundary: the slot is a dead end
                if p[0] in side_of:              # internal redex wire: slot <-> slot
                    union(("slot", side, i), ("slot", side_of[p[0]], p[1]))
                else:                            # external partner: a hard end
                    union(("slot", side, i), ("port", p))

        net.remove_agent(a.id)
        net.remove_agent(b.id)
        for ag in rw.new_agents:
            net.add(ag)
        chains: dict = {}
        for x in parent:
            if x[0] == "port":
                chains.setdefault(find(x), []).append(x[1])
        for ports in chains.values():
            if len(ports) == 2:
                net.connect(ports[0], ports[1])
            elif len(ports) > 2:
                raise ReduceError(
                    f"rule for {set((ga, gb))} is non-linear: {len(ports)} ports "
                    f"({sorted(ports)}) resolved into a single wire chain")
            # 1 port: that end stays free; 0 ports: a closed loop — it vanishes

    # ── reduce to normal form ──────────────────────────────────────────────────────
    def reduce(self, net: Net, *, max_steps: int = 100_000,
               opaque: Optional[set] = None,
               pick: Optional[Callable[[list], tuple]] = None,
               strict_locality: bool = False) -> Net:
        """Reduce `net` to normal form, returning a **new** net (source untouched).
        `opaque` freezes agents by id or glyph; `pick` chooses the next redex from the
        available list (default: canonical first — confluence makes the choice immaterial
        on the restricted subset). `strict_locality=True` rejects internal redex wires
        (the restricted subset) instead of resolving them (the default, full Lafont).
        Raises `ReduceError` past `max_steps`."""
        work = net.copy().check()
        opq = set(opaque or ())
        choose = pick or (lambda pairs: pairs[0])
        steps = 0
        while True:
            pairs = self.active_pairs(work, opq)
            if not pairs:
                return work
            steps += 1
            if steps > max_steps:
                raise ReduceError(
                    f"exceeded max_steps={max_steps}; the rule set is non-terminating on "
                    f"this net (interaction combinators are Turing-complete — supply a "
                    f"terminating rule set, or mark agents opaque).")
            a_id, b_id = choose(pairs)
            self._fire(work, a_id, b_id, strict_locality)

    # ── a confluence/conflict report for the restricted subset ────────────────────
    def validate(self, signatures: Optional[dict] = None) -> list[str]:
        """Report potential confluence hazards. By construction there is ≤1 rule per glyph
        pair (uniqueness is structural). With `signatures`, also flags `annihilate` pairs
        whose glyphs have mismatched arity (the cross-link would be ill-formed)."""
        issues = []
        for key, (ga, gb, fn) in self.rules.items():
            if signatures and getattr(fn, "_annihilate", False):
                if signatures.get(ga, 0) != signatures.get(gb, 0):
                    issues.append(f"annihilate rule {set(key)}: arity "
                                  f"{signatures.get(ga,0)} != {signatures.get(gb,0)}")
        return issues


# ── rule constructors ──────────────────────────────────────────────────────────────
def annihilate(*, swap: bool = False) -> Rule:
    """The classic *annihilation*: a same-glyph active pair vanishes, cross-linking matched
    aux ports. Two flavors, and Lafont's calculus needs both — the asymmetry is what makes
    the combinators universal: `swap=False` links index-straight `A(i) <-> B(i)` (δδ, the
    crossing picture); `swap=True` links index-reversed `A(i) <-> B(n+1-i)` (γγ, drawn
    between mirrored bodies — the parallel-arcs picture). Reversal is an involution, so
    either flavor is symmetric in the pair's order. Both agents must share arity; for ε
    (arity 0) the pair simply disappears — erasure."""
    def fn(a: Agent, b: Agent, fresh) -> Rewrite:
        if a.arity != b.arity:
            raise ReduceError(f"annihilate needs equal arity: {a.glyph}/{b.glyph} "
                              f"have {a.arity}/{b.arity}")
        n = a.arity
        return Rewrite(links=[(A(i), B(n + 1 - i if swap else i))
                              for i in range(1, n + 1)])
    fn._annihilate = True  # type: ignore[attr-defined]
    return fn


def commute() -> Rule:
    """The classic *commutation*: distinct-glyph principals meeting spawn a grid of copies
    (this is how duplication/structure propagates through a net). For α (arity m) meeting
    β (arity n): m copies of β take α's external aux ports, n copies of α take β's, and the
    copies interconnect in an m×n grid."""
    def fn(a: Agent, b: Agent, fresh) -> Rewrite:
        m, n = a.arity, b.arity
        # m copies of b (one per a's aux port), n copies of a (one per b's aux port)
        bcopies = [Agent(fresh(), b.glyph, n, dict(b.content)) for _ in range(m)]
        acopies = [Agent(fresh(), a.glyph, m, dict(a.content)) for _ in range(n)]
        links: list[tuple] = []
        for k in range(m):                       # b-copy k -> a's external aux (k+1)
            links.append(((bcopies[k].id, 0), A(k + 1)))
        for j in range(n):                       # a-copy j -> b's external aux (j+1)
            links.append(((acopies[j].id, 0), B(j + 1)))
        for j in range(n):                       # the m×n internal grid
            for k in range(m):
                links.append(((acopies[j].id, k + 1), (bcopies[k].id, j + 1)))
        return Rewrite(new_agents=bcopies + acopies, links=links)
    return fn


def patch(*, keep: str, set_fields: dict | None = None,
          copy_fields: dict | None = None) -> Rule:
    """The **content** rule: a pair meets, one side survives with patched content.

        {"glyphs": ["dye", "cloth"], "rule": "patch", "keep": "cloth",
         "copy": {"colour": "colour"},        # survivor field <- the consumed agent's
         "set":  {"dyed": true}}              # survivor field <- a literal

    `annihilate` and `commute` are structural: neither writes content (commute copies
    it), so *"the equipped dye makes the shirt purple"* had to be a code-registered
    `expand` — which put a game's **structure** in data and its **content** behind a
    recompile. Void Unity measured that inversion on 2026-08-29 and it is the right way
    round now: the half a designer changes on a Tuesday afternoon is the half that was
    compiled in.

    Semantics, deliberately weaker than an arbitrary rewrite so the confluence guard
    still means something:

    - The pair is consumed. The **survivor** is the agent whose glyph is `keep`; it comes
      back with the **same id, glyph, arity, tags and aux wiring**, and *only* its content
      changed. `patch` is content-only — that is the whole of its contract, and it is what
      makes it safe to reason about.
    - Content is `survivor.content`, then `copy` (each `target: source` reads the
      **consumed** agent's `source`, skipped when absent), then `set` (literals, last, so
      they win). Neither map may be empty of effect *and* absent — a patch that writes
      nothing is a rule that says the pair vanishes, which is what `annihilate` is for.
    - The survivor's **principal is left free** (the thing it met is gone) and the consumed
      agent's aux partners are freed. So `patch` never creates an active pair, which is why
      it cannot make a terminating rule set non-terminating, and why a second dye cannot
      queue on the same garment: nothing rewires to a free principal.
    - **The glyphs must differ.** On an unordered same-glyph pair, "which side survives" has
      no answer that is not an arbitrary tiebreak, and an arbitrary tiebreak on an unordered
      pair is exactly the schedule-dependence the derived-id rule exists to remove. Refused
      at spec-compile time rather than resolved.

    **It keeps the survivor's id, and that is a deliberate exception to the derived-id
    rule** (contract §2): §2 governs an agent a rule *creates*, and `patch` creates none —
    it hands one back. Keeping the id is both the honest reading (the shirt is still the
    shirt: same glyph, same arity, same wiring, same tags) and the one that preserves a
    boxed agent's `<rune>/` provenance for free, which is the case a host most often needs
    to draw. It stays schedule-independent because it is an *input* id."""
    def fn(a: Agent, b: Agent, fresh) -> Rewrite:
        if a.glyph == b.glyph:
            raise ReduceError(
                f"patch needs distinct glyphs; {a.glyph!r} met itself, and `keep` cannot "
                f"say which side of an unordered same-glyph pair survives")
        survivor, consumed = (a, b) if keep == a.glyph else (b, a)
        content = dict(survivor.content)
        for target, source in (copy_fields or {}).items():
            if source in consumed.content:
                content[target] = consumed.content[source]
        content.update(set_fields or {})
        out = Agent(survivor.id, survivor.glyph, survivor.arity, content,
                    list(survivor.tags))
        ref = A if survivor is a else B
        return Rewrite(new_agents=[out],
                       links=[((out.id, i), ref(i))
                              for i in range(1, survivor.arity + 1)])
    return fn


def expand(build: Callable[[Agent, Agent, Callable[[], str]], Rewrite]) -> Rule:
    """A reference-expansion rule (Fountain's 'inline a fragment'): the active pair is
    consumed and replaced by whatever subnet `build` returns. Transient by definition —
    re-derived each reduction, never written to owned state (that's `materialize`, in Scry)."""
    return build
