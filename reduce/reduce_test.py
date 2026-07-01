"""
reduce_test.py — Reduce: the interaction-net laws + worked rewrites.

Covers identity, normal form, **strong confluence under randomized schedules** (the
diamond property — the whole point of the restricted form), the termination guard,
locality/linearity preservation, opaque pass-through, purity, and worked annihilation /
commutation / expand examples, plus the mantle <-> net adapter.

    python reduce/reduce_test.py
"""
from __future__ import annotations

import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from net import Agent, Net, NetError, from_net, to_net  # noqa: E402
from reduce import (  # noqa: E402
    A, B, Reducer, ReduceError, Rewrite, annihilate, commute, expand,
)

# Signatures for the interaction combinators γ(con)=2, δ(dup)=2, ε(era)=0, plus an inert leaf.
SIG = {"con": 2, "dup": 2, "era": 0, "leaf": 0}


def combinators() -> Reducer:
    """The classic confluent system: same-glyph annihilate, distinct-glyph commute."""
    r = Reducer()
    for g in ("con", "dup", "era"):
        r.rule(g, g, annihilate())
    r.rule("dup", "con", commute())
    r.rule("con", "era", commute())
    r.rule("dup", "era", commute())
    return r


def confluence_net() -> Net:
    """δ meets γ at principals; δ's aux go to erasers, γ's aux to inert leaves."""
    n = Net()
    n.add(Agent("d", "dup", 2))
    n.add(Agent("c", "con", 2))
    n.add(Agent("e1", "era", 0)); n.add(Agent("e2", "era", 0))
    n.add(Agent("L1", "leaf", 0)); n.add(Agent("L2", "leaf", 0))
    n.connect(("d", 0), ("c", 0))          # the active pair (commute)
    n.connect(("d", 1), ("e1", 0)); n.connect(("d", 2), ("e2", 0))
    n.connect(("c", 1), ("L1", 0)); n.connect(("c", 2), ("L2", 0))
    return n.check()


def main() -> int:
    R = combinators()

    # ── identity: no active pair (or no rule) ⇒ net unchanged ──────────────────────
    flat = Net()
    flat.add(Agent("a", "leaf")); flat.add(Agent("b", "leaf"))
    flat.connect(("a", 0), ("b", 0))           # leaf~leaf has no rule
    assert R.reduce(flat).canonical() == flat.canonical()
    assert Reducer().reduce(confluence_net()).canonical() == confluence_net().canonical()

    # ── worked annihilation: con~con vanishes, cross-linking the leaves ───────────
    ann = Net()
    ann.add(Agent("x", "con", 2)); ann.add(Agent("y", "con", 2))
    for lf in ("p", "q", "r", "s"):
        ann.add(Agent(lf, "leaf", 0))
    ann.connect(("x", 0), ("y", 0))
    ann.connect(("x", 1), ("p", 0)); ann.connect(("x", 2), ("q", 0))
    ann.connect(("y", 1), ("r", 0)); ann.connect(("y", 2), ("s", 0))
    out = R.reduce(ann).check()
    assert "x" not in out.agents and "y" not in out.agents      # pair consumed
    assert out.partner(("p", 0)) == ("r", 0)                    # A(1)<->B(1)
    assert out.partner(("q", 0)) == ("s", 0)                    # A(2)<->B(2)
    assert len(out.agents) == 4                                  # only leaves remain

    # ── worked commutation: dup~con spawns the 2×2 grid of copies ─────────────────
    com = Net()
    com.add(Agent("d", "dup", 2)); com.add(Agent("c", "con", 2))
    for lf in ("a1", "a2", "b1", "b2"):
        com.add(Agent(lf, "leaf", 0))
    com.connect(("d", 0), ("c", 0))
    com.connect(("d", 1), ("a1", 0)); com.connect(("d", 2), ("a2", 0))
    com.connect(("c", 1), ("b1", 0)); com.connect(("c", 2), ("b2", 0))
    # commute alone (no era rules fire): 2 con-copies + 2 dup-copies + 4 leaves
    Ronly = Reducer().rule("dup", "con", commute())
    cout = Ronly.reduce(com).check()
    glyphs = sorted(a.glyph for a in cout.agents.values())
    assert glyphs == ["con", "con", "dup", "dup", "leaf", "leaf", "leaf", "leaf"], glyphs

    # ── STRONG CONFLUENCE: every reduction order reaches the same normal form ─────
    canon = R.reduce(confluence_net()).check()
    fps = {canon.canonical()}
    for seed in range(40):
        rng = random.Random(seed)
        nf = R.reduce(confluence_net(), pick=lambda ps: rng.choice(ps)).check()
        fps.add(nf.canonical())
    assert len(fps) == 1, f"NOT confluent: {len(fps)} distinct normal forms"
    # and the normal form is genuinely non-trivial (two dups survive on the leaves)
    assert sorted(a.glyph for a in canon.agents.values()).count("dup") == 2

    # ── termination guard: a self-regenerating rule raises within budget ──────────
    def loopfn(a, b, fresh):
        l1, l2 = Agent(fresh(), "loop", 0), Agent(fresh(), "loop", 0)
        return Rewrite(new_agents=[l1, l2], links=[((l1.id, 0), (l2.id, 0))])
    loop = Net()
    loop.add(Agent("u", "loop", 0)); loop.add(Agent("v", "loop", 0))
    loop.connect(("u", 0), ("v", 0))
    try:
        Reducer().rule("loop", "loop", loopfn).reduce(loop, max_steps=50)
        assert False, "expected ReduceError"
    except ReduceError:
        pass

    # ── opaque pass-through: a frozen glyph is never reduced ───────────────────────
    frozen = R.reduce(confluence_net(), opaque={"dup"})
    assert frozen.canonical() == confluence_net().canonical()   # nothing fired

    # ── purity: reducing never mutates the source net ─────────────────────────────
    src = confluence_net()
    before = src.canonical()
    _ = R.reduce(src)
    assert src.canonical() == before, "reduce mutated its input net!"

    # ── locality/linearity: the normal form is a well-formed net ──────────────────
    canon.check()  # raises NetError on any non-symmetric / out-of-range wire

    # ── expand (Fountain-style reference inlining) ────────────────────────────────
    def inline(ref, tgt, fresh):
        # replace a `ref`(1)–`def`(1) pair with a fresh `body` carrying def's content,
        # wiring the body to ref's external aux port.
        body = Agent(fresh(), "body", 1, dict(tgt.content))
        return Rewrite(new_agents=[body], links=[((body.id, 0), A(1))])
    Rx = Reducer().rule("ref", "def", expand(inline))
    xn = Net()
    xn.add(Agent("r", "ref", 1)); xn.add(Agent("d", "def", 1, {"text": "hi"}))
    xn.add(Agent("host", "leaf", 1)); xn.add(Agent("anchor", "leaf", 0))
    xn.connect(("r", 0), ("d", 0)); xn.connect(("r", 1), ("host", 1))
    xn.connect(("d", 1), ("anchor", 0))
    xout = Rx.reduce(xn).check()
    body = next(a for a in xout.agents.values() if a.glyph == "body")
    assert body.content == {"text": "hi"}
    assert xout.partner((body.id, 0)) == ("host", 1)            # inlined at ref's port

    # ── mantle <-> net adapter ────────────────────────────────────────────────────
    mantle = {
        "runes": [
            {"spirit": {"name": "x"}, "glyph": "con", "content": {}},
            {"spirit": {"name": "y"}, "glyph": "con", "content": {}},
            {"spirit": {"name": "p"}, "glyph": "leaf", "content": {}},
            {"spirit": {"name": "q"}, "glyph": "leaf", "content": {}},
        ],
        "layout": {"edges": [
            {"from": "x", "to": "y", "relation": "0:0"},
            {"from": "x", "to": "p", "relation": "1:0"},
            {"from": "y", "to": "q", "relation": "1:0"},
        ]},
    }
    net = to_net(mantle, SIG)
    reduced = R.reduce(net).check()
    derived = from_net(reduced, mantle_name="m2")
    assert derived["name"] == "m2" and len(derived["runes"]) == 2  # x,y gone; p,q remain
    # strict adapter rejects a port-less edge relation
    try:
        to_net({"runes": [{"spirit": {"name": "a"}, "glyph": "leaf"}],
                "layout": {"edges": [{"from": "a", "to": "a", "relation": "child"}]}}, SIG)
        assert False, "expected NetError on a non-port relation"
    except NetError:
        pass

    print("REDUCE: OK (identity, annihilation, commutation, CONFLUENCE x40 schedules,",
          "termination guard, opaque, purity, linearity, expand, mantle adapter)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
