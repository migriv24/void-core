"""
box_verb_test.py — the `reduce` verb over a BOXED mantle, end to end through the seam.

    python voidcore/box_verb_test.py

`reduce/box_test.py` tests composition against nets built by hand. This one drives the
whole path a host actually uses: real mantles in a real state document, the box spec
authored as **data** in `config.transform`, loaded with `load_from_config()`, and the
`reduce` verb run over the world.

The scenario is the one that forced this: a player is a mantle of body parts and clothing
AND a rune in the world; an amulet in the world recolours a shirt inside the player.
"""
from __future__ import annotations

import json
import os
import sys

# Run straight from a clone: `python voidcore/<name>.py` puts THIS directory on
# sys.path, not the repo root, so the top-level `voidcore` package would not
# resolve without an editable install. Put the repo root first ourselves.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# The package wires the layer paths itself (voidcore/__init__.py); importing it first
# keeps `bindings/python/voidcore.py` from shadowing the package name.
from voidcore import (Agent, B, Box, Dispatcher, Reducer, Rewrite, VoidCore,  # noqa: E402
                      boxes_from_spec, expand)

FAIL: list[str] = []


def check(cond, msg):
    if not cond:
        FAIL.append(msg)


# The rule set: a dye meeting cloth recolours it. `expand`, because a content change is
# neither annihilate nor commute — the data rule set cannot express one yet.
def dye_rule():
    def build(dye: Agent, cloth: Agent, fresh) -> Rewrite:
        out = Agent(fresh(), cloth.glyph, cloth.arity,
                    dict(cloth.content, colour=dye.content.get("colour", "?")),
                    list(cloth.tags))
        return Rewrite(new_agents=[out],
                       links=[((out.id, i), B(i)) for i in range(1, cloth.arity + 1)])
    return expand(build)


SPEC = {
    "signatures": {"body": 1, "cloth": 2, "sound": 0, "dye": 1},
    "boxes": {"player": {"mantle": "player",
                         "interface": ["skin:0", "shirt:0", "voice:0", "shirt:2"]}},
    "rules": [],
}


def build() -> tuple[VoidCore, Dispatcher]:
    vc = VoidCore(state={"version": 1, "config": {"transform": {"reduce": SPEC}}})
    for c in [
        # the player's own mantle: a shirt worn on skin, and a voice
        "mantle new player",
        "rune new text skin", "setjson skin tone '\"pale\"'",
        "rune new text shirt", "setjson shirt colour '\"white\"'", "tag shirt +worn",
        "rune new text voice", "setjson voice pitch '\"mid\"'",
        "link shirt skin --relation 1:1",
        # the world: the player is ONE rune here, next to an amulet
        "mantle new world",
        "rune new text p1",
        "rune new text amulet", "setjson amulet colour '\"purple\"'",
        "link amulet p1 --relation 0:1",     # the dye's principal -> the box's port 1
    ]:
        r = vc.dispatch(c)
        assert r["ok"], (c, r["lines"])
    # glyphs: the C core's built-in registry is `text`, so the reduction glyphs ride as
    # the rune's glyph field via export/import rather than `rune new <glyph>`.
    st = vc.export_state()
    glyphs = {"skin": "body", "shirt": "cloth", "voice": "sound",
              "p1": "player", "amulet": "dye"}
    for m in st["mantles"]:
        for r in m["runes"]:
            r["glyph"] = glyphs.get(r["spirit"]["name"], r["glyph"])
    vc.close()
    vc = VoidCore(state=st)
    d = Dispatcher(vc).load_from_config()
    d.use_reducer(Reducer().rule("dye", "cloth", dye_rule()),
                  SPEC["signatures"], boxes_from_spec(SPEC))
    return vc, d


def test_spec_compiles():
    boxes = boxes_from_spec(SPEC)
    check(list(boxes) == ["player"], f"boxes_from_spec: {boxes}")
    check(boxes["player"] == Box("player", ("skin:0", "shirt:0", "voice:0", "shirt:2")),
          f"box compiled wrong: {boxes['player']}")
    check(boxes_from_spec({"signatures": {}}) == {},
          "a spec with no boxes should compile to {} (the flat adapter)")
    for bad, why in [({"boxes": {"p": {}}}, "missing mantle"),
                     ({"boxes": {"p": {"mantle": "m", "interface": "skin:0"}}},
                      "interface not a list")]:
        try:
            boxes_from_spec(bad)
            FAIL.append(f"boxes_from_spec accepted {why}")
        except ValueError:
            pass


def test_load_from_config():
    """The box spec rides in the state document, so it reloads with the data it governs."""
    vc, _ = build()
    d = Dispatcher(vc).load_from_config()
    check(list(d._boxes) == ["player"],
          f"load_from_config did not pick up `boxes`: {d._boxes}")
    vc.close()


def test_reduce_over_a_box():
    vc, d = build()
    r = d.dispatch("reduce --into after")
    check(r["ok"], f"reduce failed: {r['lines']}")
    derived = r["data"]

    # the world had 2 runes; composed it is 4 agents (3 player + amulet)
    check("4 agents composed" in r["lines"][0],
          f"line should report the composition: {r['lines'][0]!r}")

    by_glyph = {}
    for rn in derived["runes"]:
        by_glyph.setdefault(rn["glyph"], []).append(rn)

    cloth = by_glyph.get("cloth", [])
    check(len(cloth) == 1, f"expected one shirt, got {len(cloth)}")
    check(cloth and cloth[0]["content"].get("colour") == "purple",
          f"the shirt was not recoloured through the box: "
          f"{[c['content'] for c in cloth]}")
    check(cloth and cloth[0]["tags"] == ["worn"], "the shirt stopped being worn")
    check("dye" not in by_glyph, "the amulet should have been consumed")

    # the player's untouched parts came through, still namespaced by their rune
    names = {rn["spirit"]["name"] for rn in derived["runes"]}
    check("p1/voice" in names and "p1/skin" in names,
          f"the player's other parts are missing or unnamespaced: {sorted(names)}")

    # SOURCE UNTOUCHED: reduce is pure and previewable, boxes or not.
    st = vc.export_state()
    player = next(m for m in st["mantles"] if m["name"] == "player")
    shirt = next(r for r in player["runes"] if r["spirit"]["name"] == "shirt")
    check(shirt["content"]["colour"] == "white",
          "reduce mutated the sub-mantle — it must stay a preview")
    vc.close()


def test_flat_is_unchanged():
    """A host that declares no boxes gets exactly the old behaviour. Reducing the PLAYER
    mantle on its own is the flat path: it is an ordinary mantle whose runes are agents,
    and nothing about boxing existing changes what it does."""
    vc, _ = build()
    d = Dispatcher(vc)
    d.use_reducer(Reducer().rule("dye", "cloth", dye_rule()), SPEC["signatures"])
    check(vc.dispatch("use player")["ok"], "could not activate the player mantle")
    r = d.dispatch("reduce")
    check(r["ok"], f"flat reduce failed: {r['lines']}")
    names = {rn["spirit"]["name"] for rn in r["data"]["runes"]}
    check(names == {"skin", "shirt", "voice"},
          f"the player alone should reduce to its own 3 agents, unnamespaced: "
          f"{sorted(names)}")
    check("composed" not in r["lines"][0],
          f"no composition happened, so the line should not claim any: {r['lines'][0]!r}")
    vc.close()


def test_the_interface_exists_only_because_the_box_does():
    """The same world, with the box NOT declared, is an error — and the error is the
    honest one. Undeclared, `p1` is just a rune of an unknown glyph, so it has arity 0 and
    the amulet's edge addresses a port 1 that does not exist.

    This is worth pinning because it is the whole claim stated negatively: a mantle's
    interface is not something the parent can assume or reach for. It comes into existence
    when a box says which mantle this rune is, and until then there is nothing there to
    wire to."""
    vc, _ = build()
    d = Dispatcher(vc)
    d.use_reducer(Reducer().rule("dye", "cloth", dye_rule()), SPEC["signatures"])
    r = d.dispatch("reduce")
    check(not r["ok"], "an undeclared box should not silently reduce")
    check(any("out of range" in ln and "arity 0" in ln for ln in r["lines"]),
          f"the failure should name the missing port: {r['lines']}")
    vc.close()


def test_bad_box_is_a_clean_failure():
    """A box whose interface has drifted from its mantle fails the verb with the reason,
    rather than reducing something subtly wrong."""
    vc, _ = build()
    bad = json.loads(json.dumps(SPEC))
    bad["boxes"]["player"]["interface"] = ["skin:0", "shirt:0"]      # omits two
    d = Dispatcher(vc).load_specs(reduce=bad)
    d.use_reducer(Reducer().rule("dye", "cloth", dye_rule()),
                  bad["signatures"], boxes_from_spec(bad))
    r = d.dispatch("reduce")
    check(not r["ok"], "a bad interface should fail the verb")
    check(any("omits free port" in ln for ln in r["lines"]),
          f"the failure should name the drift: {r['lines']}")
    vc.close()


def run() -> int:
    for fn in (test_spec_compiles, test_load_from_config, test_reduce_over_a_box,
               test_flat_is_unchanged, test_the_interface_exists_only_because_the_box_does,
               test_bad_box_is_a_clean_failure):
        fn()
    if FAIL:
        print("BOX VERB: FAIL")
        for f in FAIL:
            print("  -", f)
        return 1
    print("BOX VERB: OK (spec compiles, load_from_config, reduce over a box, "
          "flat path unchanged, no box = no interface, bad interface fails cleanly)")
    return 0


if __name__ == "__main__":
    sys.exit(run())
