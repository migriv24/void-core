"""
effect_test.py — the host effect handler (vc_set_effect_handler) bound in Python.

Covers: save routing to the handler, the generic `effect <op>` verb returning data
(Hormiga's "holiday query -> tagged rune collection"), no-handler failure, and that the
cross-allocator return path (vc_alloc_str -> core free) doesn't crash under repeat calls.

    python bindings/python/effect_test.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from voidcore import VoidCore


def main() -> int:
    vc = VoidCore()
    calls = []

    # a stand-in holiday: op "query" returns a tagged rune collection from a fake store
    STORE = [
        {"spirit": {"name": "flier-1"}, "glyph": "image", "tags": ["flier", "event"]},
        {"spirit": {"name": "job-1"}, "glyph": "image", "tags": ["job"]},
    ]

    def handler(op, args):
        calls.append((op, args))
        if op == "query":
            expr = args["args"][0] if args.get("args") else ""
            return [r for r in STORE if expr in r["tags"]]   # naive tag match
        if op == "save":
            return {"adapter": "wrote", "mantles": len(args.get("mantles", []))}
        return None

    vc.set_effect_handler(handler)

    # ── save routes to the handler with the full state document ───────────────────
    vc.dispatch("mantle new m")
    r = vc.dispatch("save")
    assert r["ok"], r
    assert calls[-1][0] == "save"
    assert isinstance(calls[-1][1], dict) and "mantles" in calls[-1][1]  # got the state

    # ── the generic `effect` verb: a custom op returns data ───────────────────────
    r = vc.dispatch('effect query flier')
    assert r["ok"], r
    assert [x["spirit"]["name"] for x in r["data"]] == ["flier-1"], r["data"]
    assert calls[-1] == ("query", {"args": ["flier"]})

    r = vc.dispatch('effect query job')
    assert [x["spirit"]["name"] for x in r["data"]] == ["job-1"]

    # an op the handler returns None for -> "done", ok, no data
    r = vc.dispatch("effect noop")
    assert r["ok"] and r["data"] is None

    # ── hammer the return path: many round-trips through vc_alloc_str -> core free ─
    for _ in range(200):
        assert vc.dispatch("effect query flier")["ok"]

    vc.close()

    # ── no handler registered -> a clear failure (not a crash) ────────────────────
    vc2 = VoidCore()
    r = vc2.dispatch("effect query x")
    assert r["ok"] is False and "no host effect handler" in r["lines"][0]
    # save with no handler still does the model-side work
    assert vc2.dispatch("save")["ok"]
    vc2.close()

    print("EFFECT HANDLER: OK (save routing, effect verb -> data, None, 200x alloc/free, no-handler)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
