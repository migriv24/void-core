"""
conformance_test.py — the Scry tag evaluator agrees with the C core, byte for byte.

The consensus invariant (notes/reducer.md) is "tag-expression evaluation is **one
shared, tested primitive**." We have two implementations — `scry.projection.tag_match`
(pure Python) and the C core's `vc_filter_eval` (exercised via `ls --tag`). This test
builds the same runes in both, runs a battery of expressions, and asserts the Python
filter selects exactly the set the C core does. If the grammars ever drift, this fails.

    python scry/conformance_test.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                "bindings", "python"))

from projection import scry, tag_match  # noqa: E402
from voidcore import VoidCore  # noqa: E402  (the C binding)

# A small mantle exercising name-as-tag, glyph:<g>, namespaced + free tags.
RUNES = [
    {"spirit": {"id": "1", "name": "susie-intro"}, "glyph": "dialogue",
     "tags": ["chapter:2", "susie", "reviewed"]},
    {"spirit": {"id": "2", "name": "ralsei-greet"}, "glyph": "dialogue",
     "tags": ["chapter:2", "ralsei"]},
    {"spirit": {"id": "3", "name": "kris-walk-in"}, "glyph": "walk",
     "tags": ["chapter:1"]},
    {"spirit": {"id": "4", "name": "lone"}, "glyph": "walk", "tags": []},
]

EXPRS = [
    "",                          # empty matches all
    "chapter:2",
    "glyph:walk",
    "glyph:dialogue AND chapter:2",
    "susie OR ralsei",
    "NOT chapter:2",
    "chapter:2 AND NOT susie",   # implicit + explicit mix
    "chapter:2 susie",           # adjacency = implicit AND
    "(susie OR ralsei) AND chapter:2",
    "!glyph:walk",
    "chapter:1 || chapter:2",
    "ralsei-greet",              # name-as-tag
    "NOT (chapter:1 OR chapter:2)",
]


def c_select(vc: VoidCore, expr: str) -> set[str]:
    res = vc.dispatch(f'ls --tag "{expr}"' if expr else "ls")
    return set(res["data"] or [])


def py_select(expr: str) -> set[str]:
    return {r["spirit"]["name"] for r in scry(RUNES, where=expr)}


def main() -> int:
    vc = VoidCore()
    vc.dispatch("mantle new conf")
    for g in ("dialogue", "walk"):
        vc.register_glyph({"glyph": g, "label": g, "editor": "form", "fields": []})
    for r in RUNES:
        vc.dispatch(f"rune new {r['glyph']} {r['spirit']['name']}")
        if r["tags"]:
            vc.dispatch(f"tag {r['spirit']['name']} " + " ".join("+" + t for t in r["tags"]))

    failures = 0
    for expr in EXPRS:
        c, py = c_select(vc, expr), py_select(expr)
        ok = c == py
        failures += not ok
        flag = "ok " if ok else "MISMATCH"
        print(f"[{flag}] {expr!r:42}  C={sorted(c)}")
        if not ok:
            print(f"          python={sorted(py)}")
    vc.close()

    # tag_match parity spot-check (the membership primitive itself)
    assert tag_match(RUNES[0], "susie") and not tag_match(RUNES[0], "ralsei")
    assert tag_match(RUNES[0], "glyph:dialogue") and tag_match(RUNES[0], "susie-intro")

    print(f"\nCONFORMANCE: {len(EXPRS) - failures}/{len(EXPRS)} match the C core",
          "- OK" if not failures else "- DRIFT")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
