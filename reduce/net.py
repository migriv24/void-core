"""
reduce/net.py — the interaction Net: agents with ports, a wiring map, mantle adapters.

This is the representation [Reduce](okf/concepts/reduce.md) rewrites. It is the faithful
Lafont interaction net (notes/interaction-nets.md):

- An **Agent** (a rune's reduction-time view) has a **glyph** (its type), a `content`
  payload the engine never interprets, and **ports**: port **0 is the principal**; ports
  `1..arity` are **auxiliary**. The number of aux ports is the glyph's *port signature*
  (the deferred §4 groundwork — declared here as `arity`).
- A **Port** is `(agent_id, index)`. A **wire** connects exactly two ports. **Linearity**:
  every port is in at most one wire; a port in no wire is **free** (the net's boundary
  interface). The wiring is a symmetric partial map `link[port] -> port`.
- An **active pair** (a redex) is a wire joining two *principal* ports. That, and only
  that, is where reduction happens (locality).

`to_net`/`from_net` bridge a [mantle](okf/concepts/mantle.md) (runes + `layout.edges`)
and a Net. Because mantle edges connect *runes* (not ports), the adapter reads/writes the
port indices in the edge `relation` as `"i:j"` (from-port i ↔ to-port j). Edges without
that form can't be placed on specific ports, so the strict adapter rejects them with a
clear message — making the port requirement explicit rather than guessing.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Optional

Port = tuple[str, int]  # (agent_id, port_index); index 0 == principal

# The composition namespace (reduce/box.py, contract §7). A spliced agent is renamed
# `<rune>/<agent>`, so `/` is RESERVED in a reduction agent id and `to_net` refuses a
# rune whose name contains one. That refusal is what makes `box_path` — and therefore
# the derived-id owner prefix (§2) — a fact rather than a guess: without it a host with
# a rune literally named "a/b" would silently look boxed.
SEP = "/"


def box_path(agent_id: str) -> tuple[str, ...]:
    """Which box an agent came from: `"p1/hand/finger"` -> `("p1", "hand")`; a parent's
    own agent -> `()`."""
    return tuple(agent_id.split(SEP)[:-1])


class NetError(ValueError):
    """A malformed net: a port out of range, a non-symmetric or over-subscribed wire."""


@dataclass
class Agent:
    id: str
    glyph: str
    arity: int = 0                       # number of auxiliary ports (principal is extra)
    content: dict = field(default_factory=dict)
    tags: list = field(default_factory=list)  # carried through to_net/from_net verbatim;
                                              # rule-generated agents start tagless

    def ports(self) -> list[Port]:
        return [(self.id, i) for i in range(self.arity + 1)]

    def principal(self) -> Port:
        return (self.id, 0)

    def aux(self, i: int) -> Port:
        if not (1 <= i <= self.arity):
            raise NetError(f"agent {self.id!r} ({self.glyph}) has no aux port {i} "
                           f"(arity {self.arity})")
        return (self.id, i)


@dataclass
class Net:
    agents: dict[str, Agent] = field(default_factory=dict)
    link: dict[Port, Port] = field(default_factory=dict)   # symmetric; free ports absent

    # ── construction ─────────────────────────────────────────────────────────────
    def add(self, agent: Agent) -> Agent:
        if agent.id in self.agents:
            raise NetError(f"duplicate agent id {agent.id!r}")
        self.agents[agent.id] = agent
        return agent

    def connect(self, p: Port, q: Port) -> None:
        """Wire two ports (symmetric). Each must currently be free."""
        self._check_port(p)
        self._check_port(q)
        if p in self.link or q in self.link:
            raise NetError(f"linearity violation: {p} or {q} already wired")
        self.link[p] = q
        self.link[q] = p

    def disconnect(self, p: Port) -> Optional[Port]:
        q = self.link.pop(p, None)
        if q is not None:
            self.link.pop(q, None)
        return q

    def partner(self, p: Port) -> Optional[Port]:
        return self.link.get(p)

    def remove_agent(self, aid: str) -> None:
        ag = self.agents.pop(aid)
        for i in range(ag.arity + 1):
            self.disconnect((aid, i))

    def copy(self) -> "Net":
        n = Net()
        n.agents = {k: Agent(v.id, v.glyph, v.arity, dict(v.content), list(v.tags))
                    for k, v in self.agents.items()}
        n.link = dict(self.link)
        return n

    # ── validation ───────────────────────────────────────────────────────────────
    def _check_port(self, p: Port) -> None:
        aid, idx = p
        ag = self.agents.get(aid)
        if ag is None:
            raise NetError(f"port references unknown agent {aid!r}")
        if not (0 <= idx <= ag.arity):
            raise NetError(f"port index {idx} out of range for {aid!r} (arity {ag.arity})")

    def check(self) -> "Net":
        """Assert well-formedness: every wire is symmetric and references valid ports.
        Returns self so it chains. Free ports (boundary) are allowed."""
        for p, q in self.link.items():
            self._check_port(p)
            self._check_port(q)
            if self.link.get(q) != p:
                raise NetError(f"non-symmetric wire: {p}->{q} but {q}->{self.link.get(q)}")
        return self

    def free_ports(self) -> list[Port]:
        return [pt for ag in self.agents.values() for pt in ag.ports()
                if pt not in self.link]

    # ── a stable signature for comparing normal forms up to agent renaming ────────
    def canonical(self) -> tuple:
        """An id-independent fingerprint of the net's *shape* (glyphs, content, wiring),
        so two reductions that differ only in generated agent ids compare equal. Used by
        the confluence law test."""
        # order-independent multiset of (glyph, sorted content items, sorted tags)
        agent_sig = sorted((a.glyph, tuple(sorted((k, repr(v)) for k, v in a.content.items())),
                            tuple(sorted(a.tags)))
                           for a in self.agents.values())
        # wires as glyph-keyed endpoints (drop ids); undirected, so sort each pair
        def endp(p: Port):
            a = self.agents[p[0]]
            return (a.glyph, p[1])
        wires = sorted(tuple(sorted((endp(p), endp(q)))) for p, q in self.link.items()
                       if p <= q)  # each undirected wire once
        return (tuple(agent_sig), tuple(wires))


# ── mantle <-> net adapters ───────────────────────────────────────────────────────
_REL = re.compile(r"^(\d+):(\d+)$")


def check_id(name: str) -> str:
    """Refuse a rune name that would be ambiguous as an agent id. `/` is reserved for the
    composition namespace (§7), and the reservation has to be enforced at the door rather
    than assumed: `box_path` and the derived-id owner prefix both read structure out of an
    id, so a rune named "a/b" in a flat mantle would otherwise read as an agent belonging
    to a box named "a". Refusing the input is this project's standing preference over
    guessing at it (SPEC §6.1 rule 5, same reasoning)."""
    if SEP in name:
        raise NetError(
            f"rune name {name!r} contains {SEP!r}, which is reserved for the composition "
            f"namespace (an agent spliced in from a boxed mantle is named "
            f"'<rune>{SEP}<agent>'). Rename the rune, or drop the {SEP!r}.")
    return name


def to_net(mantle: dict, signatures: dict[str, int]) -> Net:
    """Build a Net from a mantle (`runes` + `layout.edges`). `signatures` maps glyph ->
    aux-port count (arity). Edge ports are read from `relation` as `"i:j"`."""
    net = Net()
    for rune in mantle.get("runes", []):
        name = rune["spirit"]["name"]
        glyph = rune.get("glyph", "")
        check_id(name)
        net.add(Agent(name, glyph, signatures.get(glyph, 0),
                      dict(rune.get("content") or {}), list(rune.get("tags") or [])))
    for e in mantle.get("layout", {}).get("edges", []):
        m = _REL.match(str(e.get("relation", "")))
        if not m:
            raise NetError(
                f"edge {e.get('from')}->{e.get('to')} has relation "
                f"{e.get('relation')!r}; the reducer needs port indices as \"i:j\" "
                f"(e.g. \"0:0\" for principal-principal). See reduce/net.py.")
        i, j = int(m.group(1)), int(m.group(2))
        net.connect((e["from"], i), (e["to"], j))
    return net.check()


def from_net(net: Net, *, mantle_name: str = "reduced") -> dict:
    """Project a Net back to a mantle dict (the derived output). Each wire becomes a
    `layout.edge` with `relation="i:j"`. Undirected wires are emitted once. Agent
    `tags` (carried in by `to_net`) come back on the runes, so a to_net/from_net
    round-trip preserves them; agents created *by rules* during reduction have none."""
    runes = [{
        "spirit": {"id": f"rune_{a.id}", "name": a.id},
        "glyph": a.glyph,
        "facets": {k: "" for k in ("who", "what", "when", "where", "why", "how")},
        "tags": list(a.tags), "content": dict(a.content), "placement": None, "relations": [],
    } for a in net.agents.values()]
    edges = []
    for p, q in net.link.items():
        if p <= q:  # one direction per undirected wire
            edges.append({"from": p[0], "to": q[0], "relation": f"{p[1]}:{q[1]}",
                          "weight": 1.0, "directed": False})
    return {"id": f"mantle_{mantle_name}", "name": mantle_name, "domain": None,
            "runes": runes, "tags": {}, "layout": {"edges": edges}, "rules": []}
