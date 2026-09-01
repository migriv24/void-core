"""
reduce/box.py — net composition: a mantle appearing inside another mantle as one rune.

A mantle is a net. A net with *n* free ports **is** an agent of arity *n* — that is
Lafont's own reading, and it is the whole content of "a player is a mantle of clothing
and body parts, and also a rune in the world." So boxing needs no new primitive: it is
the adapter noticing that a rune's glyph names a mantle, and **splicing** that mantle's
net in at the rune's ports.

    world:  [dye] --0:1--> [player]        player:  [skin] [shirt] [voice]
                                                    (its free ports are its interface)

    composed:  [dye] --0:1--> [p1/shirt]   ← the dye's wire landed on an INNER agent,
                                             because port 1 of the box *is* that port

## Why this gives encapsulation and interaction at the same time

Those look like opposite requirements and are not, which is the reason this is worth
building rather than working around:

- **The outside cannot reach in**, because an inner agent's ports are already wired
  inside. Linearity (every port in at most one wire) does the work — not a filter, not a
  scope check that some future code path can forget to apply. An inner `silk` and an
  outer `silk` never form an active pair because there is no *wire* between them and
  no way to add one: the only ports the parent can address are the interface.
- **The outside can still affect the inside**, through the interface, because a wire to
  interface port *k* is a wire to a real inner port. An equipped dye meets the shirt it
  is wired to, the rule fires, and the effect propagates inward by ordinary reduction.

So "what if an external rune changes a colour inside the player" needs no escape hatch:
it is a wire to a free port, and reduction does the rest. Encapsulation here is a
*consequence* of the model rather than a rule imposed on top of it — which is what makes
it hold under every future rule set instead of until someone forgets.

## What a box declares (and what it must not)

A box declares its interface's **order**, never its membership. The free ports of the
sub-net *are* the interface; an explicit list only says which one is port 0 (principal),
which is port 1, and so on — because the parent addresses them by index, and a set has no
indices. A declaration that is not a permutation of the actual free ports is an error
(`box-interface`), so the two can never drift into disagreeing. The same reasoning bans a
`signatures` entry that contradicts a box's arity.

## Namespacing, and why by rune name

Spliced agents are renamed `<rune>/<agent>`: the *rune* is the instance, so two players
in one world (`p1`, `p2`) are two independent copies of the same mantle rather than one
shared net. Nesting composes the prefixes (`p1/hand/finger`). This is also the
composed net's provenance — see `box_path`.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Optional

from net import SEP, Agent, Net, NetError, Port, box_path, check_id, to_net

__all__ = ["SEP", "Box", "BoxError", "box_path", "compose", "interface_ports",
           "parse_port"]


class BoxError(NetError):
    """A malformed box: unknown or cyclic mantle, or an interface that is not a
    permutation of the sub-net's free ports.

    The abstract error **kind** (`conformance/reduce/README.md` §5) is carried in a
    field, not inferred from the message. A runner that sniffs the prose for keywords
    silently reclassifies a case the moment a diagnostic is reworded — and a case that
    changes kind without changing behaviour is exactly what a conformance suite exists
    to prevent. (Void Unity, 2026-08-29, who carried it in a field on first port and
    told us ours did not.)"""

    def __init__(self, message: str, kind: str = "box-interface"):
        super().__init__(message)
        self.kind = kind


@dataclass(frozen=True)
class Box:
    """One glyph that means "a rune of this glyph *is* the mantle `mantle`".

    `interface` orders the sub-net's free ports as `"<agent>:<port>"` strings — index 0
    becomes the box's principal port, the rest its auxiliaries, so the box's arity is
    `len(interface) - 1`. `None` means canonical order (sorted by agent id, then port),
    which is deterministic but arbitrary: declare the order whenever the parent's edges
    care which port is which, i.e. essentially always."""
    mantle: str
    interface: Optional[tuple[str, ...]] = None

    def arity(self, n_free: int) -> int:
        return (len(self.interface) if self.interface is not None else n_free) - 1


def parse_port(s: str) -> Port:
    """`"shirt:1"` -> `("shirt", 1)`. The agent id may itself contain `:` only if the
    port suffix is still the last one, so we split from the right."""
    agent, _, idx = str(s).rpartition(":")
    if not agent or not idx.isdigit():
        raise BoxError(f"interface entry {s!r} is not \"<agent>:<port>\"",
                       "box-interface")
    return (agent, int(idx))


def interface_ports(net: Net, order: Optional[Iterable[str]] = None) -> list[Port]:
    """The sub-net's free ports as an ordered interface.

    With no `order`, canonical (sorted). With one, it MUST be a permutation of the free
    ports — every declared port free, every free port declared, no repeats. That check is
    the whole point of letting a box declare anything: an interface is allowed to *order*
    the boundary and never to redefine it, so a sub-mantle cannot grow a port its net does
    not have, or hide one it does."""
    free = net.free_ports()
    if order is None:
        return sorted(free)
    declared = [parse_port(s) for s in order]
    if len(set(declared)) != len(declared):
        dupes = sorted({p for p in declared if declared.count(p) > 1})
        raise BoxError(f"interface repeats {', '.join(f'{a}:{i}' for a, i in dupes)}",
                       "box-interface")
    fset, dset = set(free), set(declared)
    if fset != dset:
        missing = sorted(fset - dset)
        extra = sorted(dset - fset)
        parts = []
        if missing:
            parts.append("omits free port(s) "
                         + ", ".join(f"{a}:{i}" for a, i in missing))
        if extra:
            parts.append("names non-free port(s) "
                         + ", ".join(f"{a}:{i}" for a, i in extra))
        raise BoxError("interface is not a permutation of the sub-net's free ports: "
                       + "; ".join(parts) + ". An interface orders the boundary, it "
                       "cannot redefine it.", "box-interface")
    return declared


def compose(mantle: dict, signatures: dict[str, int], *,
            boxes: Optional[dict[str, Box]] = None,
            mantles: Optional[dict[str, dict]] = None,
            _chain: tuple[str, ...] = ()) -> Net:
    """Build the net of `mantle`, splicing in any rune whose glyph names a box.

    With no boxes this is exactly `to_net` (same net, same errors), so composition is a
    superset of the flat adapter and a host pays nothing for not using it.

    `mantles` maps mantle name -> mantle dict; a box resolves through it. Nesting is
    recursive and a mantle that (transitively) contains itself is rejected rather than
    expanded forever."""
    boxes = boxes or {}
    if not boxes:
        return to_net(mantle, signatures)
    mantles = mantles or {}

    net = Net()
    # (rune name, port index) -> the real port in the composed net. For an ordinary rune
    # that is itself; for a box it is the spliced sub-net's k-th interface port.
    port_map: dict[Port, Port] = {}
    arity_of: dict[str, int] = {}

    for rune in mantle.get("runes", []):
        name = rune["spirit"]["name"]
        glyph = rune.get("glyph", "")
        check_id(name)
        box = boxes.get(glyph)
        if box is None:
            ag = net.add(Agent(name, glyph, signatures.get(glyph, 0),
                               dict(rune.get("content") or {}),
                               list(rune.get("tags") or [])))
            arity_of[name] = ag.arity
            for i in range(ag.arity + 1):
                port_map[(name, i)] = (name, i)
            continue

        # ── a box: compose the sub-mantle and splice it in under this rune's name ──
        if box.mantle in _chain:
            raise BoxError(f"box cycle: mantle {box.mantle!r} contains itself via "
                           f"{' -> '.join(_chain + (box.mantle,))}", "box-cycle")
        sub_mantle = mantles.get(box.mantle)
        if sub_mantle is None:
            raise BoxError(f"rune {name!r} has box glyph {glyph!r}, but no mantle "
                           f"{box.mantle!r} is available to splice in", "box-mantle")
        sub = compose(sub_mantle, signatures, boxes=boxes, mantles=mantles,
                      _chain=_chain + (box.mantle,))
        iface = interface_ports(sub, box.interface)
        if not iface:
            raise BoxError(f"mantle {box.mantle!r} has no free ports, so it has no "
                           f"interface and cannot appear as a rune (a net with n free "
                           f"ports is an agent of arity n; n=0 leaves nothing to wire)",
                           "box-interface")
        declared = signatures.get(glyph)
        if declared is not None and declared != len(iface) - 1:
            raise BoxError(f"signatures says glyph {glyph!r} has arity {declared}, but "
                           f"its box interface has {len(iface) - 1} aux port(s). A box's "
                           f"arity comes from its net; drop the signatures entry.",
                           "box-interface")

        pre = name + SEP
        for aid, ag in sub.agents.items():
            net.add(Agent(pre + aid, ag.glyph, ag.arity, dict(ag.content), list(ag.tags)))
        for p, q in sub.link.items():
            if p <= q:
                net.connect((pre + p[0], p[1]), (pre + q[0], q[1]))
        arity_of[name] = len(iface) - 1
        for k, (aid, idx) in enumerate(iface):
            port_map[(name, k)] = (pre + aid, idx)

    # ── the parent's own edges, resolved through the map ──────────────────────────
    for e in mantle.get("layout", {}).get("edges", []):
        i, j = _edge_ports(e)
        src, dst = (e["from"], i), (e["to"], j)
        for (ref, port) in (src, dst):
            if ref not in arity_of:
                # NOT a BoxError. `to_net` reports this as `adapter-ports` (the port
                # references an unknown agent), and it is the same malformed edge here —
                # so reporting it as a box problem would make one defect answer to two
                # abstract kinds depending only on whether some OTHER rune in the mantle
                # happened to be a box. (Void Unity, 2026-08-29, who found it by porting
                # the runner rather than by reading it; case 21 now locks both paths.)
                raise NetError(f"edge {e['from']}->{e['to']} references {ref!r}, which is "
                               f"not a rune of this mantle")
            if not (0 <= port <= arity_of[ref]):
                raise NetError(f"edge {e['from']}->{e['to']} uses port {port} of {ref!r}, "
                               f"which has arity {arity_of[ref]}")
        net.connect(port_map[src], port_map[dst])
    return net.check()


def _edge_ports(e: dict) -> tuple[int, int]:
    """Port indices out of an edge's `relation` (`"i:j"`) — the same strict reading the
    flat adapter uses, quoted here rather than shared so `net.to_net` stays the contract
    §4 describes, unchanged by composition existing."""
    from net import _REL
    m = _REL.match(str(e.get("relation", "")))
    if not m:
        raise NetError(
            f"edge {e.get('from')}->{e.get('to')} has relation {e.get('relation')!r}; "
            f"the reducer needs port indices as \"i:j\" (e.g. \"0:0\" for "
            f"principal-principal). See reduce/net.py.")
    return int(m.group(1)), int(m.group(2))


# ── provenance ────────────────────────────────────────────────────────────────────
# `box_path` lives in net.py (it reads structure out of an agent id, which is a net-level
# fact) and is re-exported here, where the structure is created. What it answers:
#
#   - a **surviving** agent carries its `<rune>/` prefix, so its box is read straight off
#     the id;
#   - an agent **created by a rule** is named from the redex, and as of 0.2.12 it inherits
#     the **longest common box path of its two parents** (`reduce._owner_prefix`) — so an
#     effect firing wholly inside one character belongs to that character, which is the
#     common case in a game. Parents from two different boxes share no path and get no
#     prefix: ambiguity is reported by saying nothing rather than resolved by guessing.
#
# What it still does not answer is re-boxing a whole normal form back into sub-mantles,
# and that is deliberate rather than pending — after a rewrite spanning a boundary there
# is often no fact of the matter about which side a new agent belongs to, and a host handed
# a confident wrong answer is worse off than one handed none.
