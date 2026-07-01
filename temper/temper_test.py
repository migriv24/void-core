"""
temper_test.py — Temper: the idempotence law, purity, and the PM invariants.

    python temper/temper_test.py
"""
from __future__ import annotations

import copy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from temper import (  # noqa: E402
    Temper, dedupe, default_content, default_tag, member_or_default,
    normalize_tags, single_tag,
)


def rune(name, *, tags=None, content=None):
    return {"spirit": {"id": name, "name": name}, "glyph": "project",
            "tags": list(tags or []), "content": dict(content or {})}


def main() -> int:
    t = Temper([
        dedupe("images"),
        member_or_default("thumb", "images"),
        default_content("subtitle", ""),
        default_tag("status", "complete"),
        single_tag("status"),
        normalize_tags(),
    ])

    # identity: a rune-less Temper returns the rune untouched
    r0 = rune("x", tags=["a"], content={"images": ["1.png"]})
    assert Temper().rune(r0) == r0

    # thumb = images[0] when unset  (PM add_image's "if not thumb" branch)
    r = t.rune(rune("a", content={"images": ["a.png", "b.png"]}))
    assert r["content"]["thumb"] == "a.png", r["content"]

    # thumb pointing at a removed image is reset  (PM remove_image branch)
    r = t.rune(rune("b", content={"images": ["b.png"], "thumb": "gone.png"}))
    assert r["content"]["thumb"] == "b.png"

    # thumb already valid is preserved
    r = t.rune(rune("c", content={"images": ["a.png", "b.png"], "thumb": "b.png"}))
    assert r["content"]["thumb"] == "b.png"

    # empty images -> thumb None
    r = t.rune(rune("d", content={"images": [], "thumb": "x.png"}))
    assert r["content"]["thumb"] is None

    # image dedupe, order-preserving
    r = t.rune(rune("e", content={"images": ["a.png", "a.png", "b.png"]}))
    assert r["content"]["images"] == ["a.png", "b.png"]

    # default content field
    r = t.rune(rune("f", content={"images": ["a.png"]}))
    assert r["content"]["subtitle"] == ""

    # status defaulting + tag dedupe
    r = t.rune(rune("g", tags=["susie", "susie"]))
    assert "status:complete" in r["tags"]
    assert r["tags"].count("susie") == 1

    # explicit status preserved; contradictory second status dropped
    r = t.rune(rune("h", tags=["status:active", "status:archived"]))
    assert "status:active" in r["tags"] and "status:archived" not in r["tags"]

    # ── the idempotence LAW: temper(temper(x)) == temper(x) ───────────────────────
    samples = [
        rune("s1", content={"images": ["a.png", "a.png"], "thumb": "gone.png"}),
        rune("s2", tags=["x", "x", "status:a", "status:b"]),
        rune("s3", content={"images": []}),
        rune("s4", tags=["only"], content={"images": ["z.png"], "thumb": "z.png"}),
    ]
    for s in samples:
        once = t.rune(s)
        twice = t.rune(once)
        assert once == twice, f"NOT idempotent for {s['spirit']['name']}: {once} != {twice}"

    # ── purity: the source samples were never mutated ─────────────────────────────
    snapshot = copy.deepcopy(samples)
    _ = t.runes(samples)
    assert samples == snapshot, "temper mutated its input!"

    print("TEMPER: OK (thumb/dedupe/default/tag rules, idempotence law, purity)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
