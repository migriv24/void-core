"""
box_test.py — net composition (`reduce/box.py`): a mantle as a rune inside another mantle.

    python reduce/box_test.py

The scenario throughout is the one that forced this to exist: a **player** is a mantle of
body parts and clothing that interact among themselves, and is *also* a single rune in the
**world** mantle. Then an item from outside the player changes the colour of clothing
inside it. Those two look like opposite requirements — encapsulation, and reach-in — and
the point of these tests is that they are one mechanism seen twice.
"""
from __future__ import annotations

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from box import Box, BoxError, box_path, compose, interface_ports          # noqa: E402
from net import Agent, NetError, to_net                                    # noqa: E402
from reduce import B, Reducer, ReduceError, Rewrite, expand, patch      # noqa: E402

FAIL: list[str] = []


def check(cond, msg):
    if not cond:
        FAIL.append(msg)


def raises(exc, fn, *, contains=None, label=""):
    try:
        fn()
    except exc as e:
        if contains and contains not in str(e):
            FAIL.append(f"{label}: raised {exc.__name__} but not about {contains!r}: {e}")
        return
    except Exception as e:                                          # noqa: BLE001
        FAIL.append(f"{label}: raised {type(e).__name__}, expected {exc.__name__}: {e}")
        return
    FAIL.append(f"{label}: expected {exc.__name__}, nothing raised")


# ── fixtures ──────────────────────────────────────────────────────────────────────
def rune(name, glyph, content=None, tags=None):
    return {"spirit": {"id": f"rune_{name}", "name": name}, "glyph": glyph,
            "content": content or {}, "tags": tags or []}


def mantle(name, runes, edges=()):
    return {"name": name, "runes": list(runes),
            "layout": {"edges": [{"from": f, "to": t, "relation": r} for f, t, r in edges]}}


# The player mantle: a body wearing a shirt, plus a voice.
#
#   skin  (body,  arity 1):  port 0 FREE — the world's handle on the player
#                            port 1 wired to shirt:1
#   shirt (cloth, arity 2):  port 0 FREE — its PRINCIPAL, i.e. the equip socket
#                            port 1 wired to skin:1  (it is worn)
#                            port 2 FREE — a spare anchor
#   voice (sound, arity 0):  port 0 FREE
#
# Which ports are free is the whole design of a mantle-as-agent, and one of them is
# load-bearing in a way that is easy to miss: an active pair is principal-to-principal,
# so a garment can only be *interacted with* from outside if its PRINCIPAL is free and in
# the interface. "What this mantle exposes for interaction" is exactly "which inner
# principals are free" — test_equip_socket_must_be_a_principal pins that.
PLAYER = mantle("player",
                [rune("skin", "body", {"tone": "pale"}),
                 rune("shirt", "cloth", {"colour": "white"}, ["worn"]),
                 rune("voice", "sound", {"pitch": "mid"})],
                [("shirt", "skin", "1:1")])

SIGS = {"body": 1, "cloth": 2, "sound": 0, "dye": 1}

# A rune of glyph "player" IS the player mantle. The interface names every free port,
# in the order the world addresses them: 0 = skin (principal of the box), 1 = the shirt's
# equip socket, 2 = voice, 3 = the shirt's spare anchor.
BOXES = {"player": Box("player", ("skin:0", "shirt:0", "voice:0", "shirt:2"))}
MANTLES = {"player": PLAYER}


# ── 1. a net with n free ports is an agent of arity n ─────────────────────────────
def test_interface():
    net = to_net(PLAYER, SIGS)
    free = set(net.free_ports())
    check(free == {("skin", 0), ("shirt", 0), ("shirt", 2), ("voice", 0)},
          f"player's free ports wrong: {sorted(free)}")

    check(interface_ports(net) == sorted(free), "canonical interface is not sorted order")

    order = ("voice:0", "skin:0", "shirt:0", "shirt:2")
    check(interface_ports(net, order) ==
          [("voice", 0), ("skin", 0), ("shirt", 0), ("shirt", 2)],
          "declared interface not honoured in order")

    # An interface ORDERS the boundary and may not redefine it: it cannot invent a port
    # the net does not have...
    raises(BoxError, lambda: interface_ports(net, ("skin:0", "shirt:0", "shirt:2",
                                                   "voice:0", "shirt:1")),
           contains="non-free port", label="interface inventing a port")
    # ...nor hide one it does...
    raises(BoxError, lambda: interface_ports(net, ("skin:0", "shirt:0")),
           contains="omits free port", label="interface hiding a port")
    # ...nor repeat one.
    raises(BoxError, lambda: interface_ports(net, ("skin:0", "skin:0", "shirt:0",
                                                   "shirt:2")),
           contains="repeats", label="interface repeating a port")


# ── 2. composition: the player is one rune in the world ───────────────────────────
def test_compose_basic():
    world = mantle("world", [rune("p1", "player"), rune("door", "body")],
                   [("p1", "door", "0:0")])          # the player's skin meets a door
    net = compose(world, SIGS, boxes=BOXES, mantles=MANTLES)

    check(set(net.agents) == {"p1/skin", "p1/shirt", "p1/voice", "door"},
          f"composed agents wrong: {sorted(net.agents)}")
    check(net.partner(("p1/skin", 0)) == ("door", 0),
          f"box port 0 did not resolve to the interface's first port: "
          f"{net.partner(('p1/skin', 0))}")
    check(net.partner(("p1/shirt", 1)) == ("p1/skin", 1), "inner wire lost in splice")

    # Two players are two independent copies — the instance is the RUNE, not the mantle.
    world2 = mantle("world", [rune("p1", "player"), rune("p2", "player")],
                    [("p1", "p2", "2:2")])           # voice to voice
    n2 = compose(world2, SIGS, boxes=BOXES, mantles=MANTLES)
    check(len(n2.agents) == 6, f"two players should splice to 6 agents, got {len(n2.agents)}")
    check(n2.partner(("p1/voice", 0)) == ("p2/voice", 0), "p1 and p2 not wired voice-to-voice")
    check(n2.partner(("p1/shirt", 1)) == ("p1/skin", 1), "p1's inner wiring leaked to p2")


def test_compose_is_a_superset():
    """With no boxes, composition IS the flat adapter — same net, same errors. A host that
    never boxes anything pays nothing for the feature existing."""
    check(to_net(PLAYER, SIGS).canonical() == compose(PLAYER, SIGS).canonical(),
          "compose() diverged from to_net() with no boxes")
    raises(NetError, lambda: compose(mantle("m", [rune("a", "body"), rune("b", "body")],
                                            [("a", "b", "holds")]), SIGS,
                                     boxes=BOXES, mantles=MANTLES),
           contains="i:j", label="composition keeps the strict port requirement")


def test_nesting():
    """A box inside a box: the prefixes compose, and `box_path` reads them back."""
    hand = mantle("hand", [rune("finger", "body")], [])
    body = mantle("body-mantle", [rune("h", "hand"), rune("torso", "body")],
                  [("h", "torso", "1:1")])
    boxes = {"hand": Box("hand", ("finger:0", "finger:1")),
             # note the namespaced entry: an outer box's interface names ports of the
             # COMPOSED sub-net, spliced ones included.
             "bodykit": Box("body-mantle", ("torso:0", "h/finger:0"))}
    world = mantle("world", [rune("guy", "bodykit"), rune("wall", "body")],
                   [("guy", "wall", "0:0")])
    net = compose(world, SIGS, boxes=boxes, mantles={"hand": hand, "body-mantle": body})
    check(set(net.agents) == {"guy/h/finger", "guy/torso", "wall"},
          f"nested compose wrong: {sorted(net.agents)}")
    check(box_path("guy/h/finger") == ("guy", "h"), "box_path lost a level")
    check(box_path("wall") == (), "box_path invented a level")
    check(net.partner(("guy/torso", 0)) == ("wall", 0), "nested interface misrouted")


def test_bad_boxes():
    world = mantle("world", [rune("p1", "player")], [])
    raises(BoxError, lambda: compose(world, SIGS, boxes={"player": Box("nope")},
                                     mantles=MANTLES),
           contains="no mantle", label="box naming a mantle that is not there")

    # A mantle that contains itself is rejected rather than expanded forever.
    loop = mantle("loop", [rune("inner", "loopbox"), rune("x", "body")], [])
    raises(BoxError, lambda: compose(mantle("world", [rune("l", "loopbox")], []), SIGS,
                                     boxes={"loopbox": Box("loop")},
                                     mantles={"loop": loop}),
           contains="cycle", label="self-containing mantle")

    # Two sources of truth for arity are refused rather than silently ranked.
    raises(BoxError, lambda: compose(world, dict(SIGS, player=9), boxes=BOXES,
                                     mantles=MANTLES),
           contains="signatures", label="signatures contradicting a box's net")


# ── 3. the encapsulation half ─────────────────────────────────────────────────────
def test_encapsulation_is_linearity():
    """The prize: an inner `shirt` and an outer `shirt` never form an active pair — and
    not because anything filtered them. The parent can address only the interface, and
    every other inner port is already wired, so by linearity there is no wire between them
    and no way to add one. That is why it holds under every future rule set instead of
    until someone forgets a check."""
    world = mantle("world",
                   [rune("p1", "player"), rune("shirt", "cloth", {"colour": "red"})], [])
    net = compose(world, SIGS, boxes=BOXES, mantles=MANTLES)

    check(net.partner(("p1/shirt", 1)) == ("p1/skin", 1),
          "the inner shirt's worn-wire is missing — encapsulation would be luck")
    check(net.partner(("p1/shirt", 0)) is None and net.partner(("shirt", 0)) is None,
          "both shirts should be unwired here")
    check(net.partner(("p1/shirt", 0)) != ("shirt", 0),
          "inner and outer shirt are an active pair")

    # The parent cannot NAME an inner port that is not in the interface: the box has 4
    # ports (0..3), so port 4 does not exist and an edge to it is an error, not a way in.
    raises(NetError, lambda: compose(
        mantle("world", [rune("p1", "player"), rune("shirt", "cloth")],
               [("p1", "shirt", "4:0")]), SIGS, boxes=BOXES, mantles=MANTLES),
        contains="arity", label="reaching past the interface")


# ── 4. the interaction half — same mechanism, seen from outside ───────────────────
def dye_rule():
    """`dye × cloth` -> the same cloth, recoloured. Registered `("dye", "cloth")`, so the
    rule is called with a=dye, b=cloth. The new cloth keeps the old one's external aux
    partners (`B(1)` is whatever the shirt was worn on), and its principal is left free —
    the equip socket reopens, so it can be dyed again.

    An `expand` rule, because a content change is neither annihilate nor commute and the
    *data* rule set cannot express it today. That gap is real and is called out in the
    reduce contract rather than papered over."""
    def build(dye: Agent, cloth: Agent, fresh) -> Rewrite:
        out = Agent(fresh(), cloth.glyph, cloth.arity,
                    dict(cloth.content, colour=dye.content.get("colour", "?")),
                    list(cloth.tags))
        return Rewrite(new_agents=[out],
                       links=[((out.id, i), B(i)) for i in range(1, cloth.arity + 1)])
    return expand(build)


def test_outside_affects_inside():
    """A dye equipped in the WORLD recolours a shirt inside the PLAYER.

    Nothing reaches in. The dye is wired to the box's port 1, which *is* `shirt:0`, so
    after composition they are an ordinary active pair in one net and the rule fires by
    ordinary reduction. This is the entire answer to "an external rune affects runes
    inside a mantle": it is a wire to a free port."""
    world = mantle("world",
                   [rune("p1", "player"), rune("amulet", "dye", {"colour": "purple"})],
                   [("amulet", "p1", "0:1")])        # the dye's principal -> shirt:0
    net = compose(world, SIGS, boxes=BOXES, mantles=MANTLES)
    check(net.partner(("amulet", 0)) == ("p1/shirt", 0),
          f"the dye did not land on the inner shirt: {net.partner(('amulet', 0))}")

    out = Reducer().rule("dye", "cloth", dye_rule()).reduce(net)

    shirts = [a for a in out.agents.values() if a.glyph == "cloth"]
    check(len(shirts) == 1, f"expected one shirt after reduction, got {len(shirts)}")
    check(shirts and shirts[0].content.get("colour") == "purple",
          f"the shirt was not recoloured: {[a.content for a in shirts]}")
    check(shirts and shirts[0].tags == ["worn"],
          f"the shirt stopped being worn: {[a.tags for a in shirts]}")
    check("amulet" not in out.agents, "the dye should have been consumed")
    # ...and it is still ON the player: the recoloured shirt kept the wire to skin.
    check(shirts and out.partner((shirts[0].id, 1)) == ("p1/skin", 1),
          "the recoloured shirt came off the body")
    # The player's other parts are untouched.
    check(out.agents["p1/voice"].content == {"pitch": "mid"}, "voice was disturbed")

    # The created shirt is named from the redex, so it carries NO box path — reduction is
    # a whole-net operation and the box is an input structure, not a partition it keeps.
    check(shirts and box_path(shirts[0].id) == (),
          "a rule-created agent should have no box path")


def test_equip_socket_must_be_a_principal():
    """The finding that came out of writing the test above, and the reason it is pinned:
    an active pair is principal-to-principal, so wiring a dye to a garment's AUX port
    composes fine and then does nothing. "What a mantle exposes for interaction" is
    exactly "which of its inner principals are free" — an interface of aux ports is a
    net that can be attached to and never reacts."""
    aux_only = {"player": Box("player", ("skin:0", "shirt:2", "voice:0", "shirt:0"))}
    world = mantle("world",
                   [rune("p1", "player"), rune("amulet", "dye", {"colour": "purple"})],
                   [("amulet", "p1", "0:1")])        # port 1 is now shirt:2, an AUX port
    net = compose(world, SIGS, boxes=aux_only, mantles=MANTLES)
    check(net.partner(("amulet", 0)) == ("p1/shirt", 2), "fixture wired the wrong port")

    out = Reducer().rule("dye", "cloth", dye_rule()).reduce(net)
    check("amulet" in out.agents and out.agents["p1/shirt"].content["colour"] == "white",
          "an aux-port wire should NOT have fired the rule")



# ── 5. provenance: a derived agent inherits its parents' common box ───────────────
def test_owner_prefix():
    """A rule-created agent is named from the redex, and as of 0.2.12 it inherits the
    LONGEST COMMON BOX PATH of its two parents. A host's job with a normal form is to
    draw it, which means answering "which entity is this agent" for every agent; a
    survivor answers with its own prefix, and before this a derived one had no answer at
    all. (Void Unity, 2026-08-29 — who asked for exactly this and explicitly did NOT ask
    for `from_net` to re-partition a normal form.)"""
    from reduce import _owner_prefix

    check(_owner_prefix("p1/silk", "p1/wand") == "p1/", "same box should give an owner")
    check(_owner_prefix("p1/h/a", "p1/h/b") == "p1/h/", "nested common path lost")
    # a shared OUTER box with different inner ones: common path is the outer one
    check(_owner_prefix("guy/h/finger", "guy/torso") == "guy/",
          "longest common path should stop at the shared level")
    # different boxes, or one unboxed: no owner. Ambiguity is reported by saying nothing.
    check(_owner_prefix("p1/silk", "p2/silk") == "", "different boxes should give no owner")
    check(_owner_prefix("p1/silk", "amulet") == "", "a cross-boundary pair has no owner")
    # THE COMPATIBILITY PROPERTY: a flat net has no prefixes anywhere, so every derived id
    # is byte-for-byte what it was before composition existed. This is what let the rule
    # ship one release after the digest was frozen.
    check(_owner_prefix("a", "b") == "", "a flat net must gain no prefix")


def test_reserved_separator():
    """`/` is reserved in a reduction agent id, and refused at the door rather than
    assumed — `box_path` and the owner prefix both read structure back out of an id, so a
    rune named "a/b" in a flat mantle would otherwise read as belonging to a box "a"."""
    bad = mantle("m", [rune("a/b", "body")], [])
    raises(NetError, lambda: to_net(bad, SIGS),
           contains="reserved", label="a slashed rune name in the flat adapter")
    raises(NetError, lambda: compose(bad, SIGS, boxes=BOXES, mantles=MANTLES),
           contains="reserved", label="a slashed rune name in the composing adapter")


# ── 6. the content rule ───────────────────────────────────────────────────────────
def test_patch_keeps_identity():
    """`patch` is content-only: same id, glyph, arity, tags and aux wiring, new content.
    Keeping the id is the point — it is what preserves a boxed agent's provenance through
    a content rewrite, which is the case a host most often has to draw."""
    world = mantle("world",
                   [rune("p1", "player"), rune("amulet", "dye", {"colour": "purple"})],
                   [("amulet", "p1", "0:1")])
    net = compose(world, SIGS, boxes=BOXES, mantles=MANTLES)
    rule = patch(keep="cloth", set_fields={"dyed": True},
                 copy_fields={"colour": "colour"})
    out = Reducer().rule("dye", "cloth", rule).reduce(net)

    check("p1/shirt" in out.agents, f"the shirt lost its id: {sorted(out.agents)}")
    shirt = out.agents["p1/shirt"]
    check(shirt.content == {"colour": "purple", "dyed": True},
          f"content not patched as specified: {shirt.content}")
    check(shirt.tags == ["worn"], f"tags are not content and must survive: {shirt.tags}")
    check(shirt.glyph == "cloth" and shirt.arity == 2, "glyph/arity changed")
    check(out.partner(("p1/shirt", 1)) == ("p1/skin", 1), "aux wiring not preserved")
    check(out.partner(("p1/shirt", 0)) is None,
          "the survivor's principal should be free — the socket reopens")
    check("amulet" not in out.agents, "the consumed agent should be gone")
    check(box_path("p1/shirt") == ("p1",), "provenance survived a content rewrite")


def test_patch_ordering_and_refusals():
    """`set` is applied after `copy`, so a literal wins over a copied field; a missing
    source field is skipped rather than written as null; and a same-glyph pair is refused
    because 'which side survives' has no non-arbitrary answer on an unordered pair."""
    a = Agent("dye1", "dye", 1, {"colour": "purple", "note": "n"})
    b = Agent("cloth1", "cloth", 2, {"colour": "white", "keepme": 1}, ["worn"])

    rw = patch(keep="cloth", copy_fields={"colour": "colour"},
               set_fields={"colour": "black"})(a, b, lambda: "unused")
    check(rw.new_agents[0].content["colour"] == "black", "`set` must win over `copy`")
    check(rw.new_agents[0].content["keepme"] == 1, "untouched survivor fields must remain")

    rw2 = patch(keep="cloth", copy_fields={"colour": "absent"})(a, b, lambda: "unused")
    check(rw2.new_agents[0].content["colour"] == "white",
          "a missing source field should be skipped, not written as null")

    same = Agent("c2", "cloth", 2, {})
    raises(ReduceError, lambda: patch(keep="cloth", set_fields={"x": 1})(b, same, lambda: "u"),
           contains="distinct glyphs", label="patch on a same-glyph pair")


def test_patch_creates_no_active_pair():
    """`patch` frees the survivor's principal and never rewires anything to it, so it
    cannot make a terminating rule set non-terminating and a second dye cannot queue on
    the same garment. Reducing twice is a fixed point."""
    world = mantle("world",
                   [rune("p1", "player"), rune("amulet", "dye", {"colour": "purple"})],
                   [("amulet", "p1", "0:1")])
    net = compose(world, SIGS, boxes=BOXES, mantles=MANTLES)
    r = Reducer().rule("dye", "cloth",
                       patch(keep="cloth", copy_fields={"colour": "colour"}))
    once = r.reduce(net)
    twice = r.reduce(once)
    check(once.canonical() == twice.canonical(), "patch did not reach a fixed point")


def run():
    for fn in (test_interface, test_compose_basic, test_compose_is_a_superset,
               test_nesting, test_bad_boxes, test_encapsulation_is_linearity,
               test_outside_affects_inside, test_equip_socket_must_be_a_principal,
               test_owner_prefix, test_reserved_separator, test_patch_keeps_identity,
               test_patch_ordering_and_refusals, test_patch_creates_no_active_pair):
        fn()
    if FAIL:
        print("BOX: FAIL")
        for f in FAIL:
            print("  -", f)
        return 1
    print("BOX: OK (interface = ordered free ports, splice, instancing, nesting, "
          "bad-box refusals, encapsulation-by-linearity, outside-affects-inside, "
          "equip socket must be a principal, owner prefix, reserved separator, "
          "patch keeps identity + ordering + fixed point)")
    return 0


if __name__ == "__main__":
    sys.exit(run())
