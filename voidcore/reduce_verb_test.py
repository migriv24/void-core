"""
reduce_verb_test.py — Reduce authored as data + the `reduce` dispatcher verb.

A reducer spec (port signatures + glyph-pair rules) compiles to a Reducer; the `reduce`
verb builds the active mantle's net (ports ride each edge's `relation` as "i:j"), reduces
to normal form, returns the derived mantle (preview), and `--commit` installs it live.

    python voidcore/reduce_verb_test.py
"""
from __future__ import annotations

import sys

from voidcore import Dispatcher, VoidCore, reduce_rule_names, reducer_from_spec

# γ(con)=2 meets γ(con)=2 at principals → annihilation; leaves are inert (no rule).
REDUCE_SPEC = {
    "signatures": {"con": 2, "leaf": 0},
    "rules": [{"glyphs": ["con", "con"], "rule": "annihilate"}],
}

# A mantle: x(con) ~ y(con) at principals (0:0); their aux ports (1,2) go to leaves.
def mantle_state():
    runes = [{"spirit": {"id": n, "name": n}, "glyph": g, "content": {}}
             for n, g in [("x", "con"), ("y", "con"),
                          ("p", "leaf"), ("q", "leaf"), ("r", "leaf"), ("s", "leaf")]]
    edges = [{"from": "x", "to": "y", "relation": "0:0", "weight": 1.0, "directed": False},
             {"from": "x", "to": "p", "relation": "1:0", "weight": 1.0, "directed": False},
             {"from": "x", "to": "q", "relation": "2:0", "weight": 1.0, "directed": False},
             {"from": "y", "to": "r", "relation": "1:0", "weight": 1.0, "directed": False},
             {"from": "y", "to": "s", "relation": "2:0", "weight": 1.0, "directed": False}]
    return {
        "version": 1, "domains": {},
        "mantles": [{"id": "m", "name": "main", "domain": None, "runes": runes,
                     "tags": {}, "layout": {"edges": edges}, "rules": []}],
        "bindings": [], "scripts": {},
        "config": {"transform": {"reduce": REDUCE_SPEC}},
        "active": {"mantle": "main", "domain": None},
    }


def main() -> int:
    # ── reducer_from_spec compiles + the conflict guard still applies ──────────────
    reducer, sigs = reducer_from_spec(REDUCE_SPEC)
    assert sigs == {"con": 2, "leaf": 0}
    assert "annihilate" in reduce_rule_names()
    try:
        reducer_from_spec({"rules": [{"glyphs": ["a", "a"], "rule": "nope"}]})
        assert False, "expected ValueError on unknown rule"
    except ValueError:
        pass

    # ── swap flavor: parses on annihilate, rejected on commute ─────────────────────
    reducer_from_spec({"signatures": {"g": 2},
                       "rules": [{"glyphs": ["g", "g"], "rule": "annihilate",
                                  "swap": True}]})
    try:
        reducer_from_spec({"rules": [{"glyphs": ["a", "b"], "rule": "commute",
                                      "swap": True}]})
        assert False, "expected ValueError on swap with commute"
    except ValueError:
        pass

    vc = VoidCore(state=mantle_state())
    for g in ("con", "leaf"):
        vc.register_glyph({"glyph": g, "label": g, "editor": "form", "fields": []})

    # the reducer is loaded from the STATE DOCUMENT (config.transform.reduce), no code
    d = Dispatcher(vc).load_from_config()

    # ── preview: reduce returns the derived mantle; source untouched ──────────────
    res = d.dispatch("reduce --into nf")
    assert res["ok"], res
    derived = res["data"]
    glyphs = sorted(r["glyph"] for r in derived["runes"])
    assert glyphs == ["leaf"] * 4, glyphs                 # x,y annihilated; 4 leaves remain
    # the two cross-links A(i)<->B(i): p-r and q-s
    pairs = {frozenset((e["from"], e["to"])) for e in derived["layout"]["edges"]}
    assert {"p", "r"} in pairs and {"q", "s"} in pairs, pairs
    # source mantle is untouched (still has x and y) — reduce is pure/previewable
    assert any(r["spirit"]["name"] == "x" for r in d._active()["runes"])

    # ── commit: installs the normal form as a live mantle ─────────────────────────
    res = d.dispatch("reduce --into nf --commit")
    assert res["ok"], res
    assert vc.dispatch("use nf")["ok"]
    assert sorted(vc.dispatch("ls")["data"]) == ["p", "q", "r", "s"]

    # unknown flag rejected
    assert d.dispatch("reduce --bogus")["ok"] is False
    vc.close()

    print("REDUCE VERB: OK (reducer_from_spec, state-doc load, preview, commit, conflict guard)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
