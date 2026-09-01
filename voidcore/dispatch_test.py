"""
dispatch_test.py — the transformation-verb seam (voidcore.Dispatcher).

Verifies it is a drop-in superset of vc.dispatch (unknown verbs delegate unchanged) and
that scry / temper / materialize behave per SPEC §7 and stay undoable.

    python voidcore/dispatch_test.py
"""
from __future__ import annotations

import os
import sys

# Run straight from a clone: `python voidcore/<name>.py` puts THIS directory on
# sys.path, not the repo root, so the top-level `voidcore` package would not
# resolve without an editable install. Put the repo root first ourselves.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from voidcore import (
    Dispatcher, Selector, Temper, VoidCore,
    dedupe, member_or_default, normalize_tags, single_tag,
)


def main() -> int:
    vc = VoidCore()
    vc.dispatch("mantle new shots")
    vc.register_glyph({"glyph": "shot", "label": "Shot", "editor": "form",
                       "fields": ["title", "images", "thumb"]})
    for name in ("alpha", "beta", "gamma"):
        vc.dispatch(f"rune new shot {name}")
    vc.dispatch("tag alpha +status:active +py +py")        # duplicate tag on purpose
    vc.dispatch("tag beta +status:active")
    vc.dispatch("tag gamma +status:archived")
    vc.dispatch("setjson alpha title 'Alpha'")
    vc.dispatch("setjson beta title 'Bravo'")     # so the title sort is deterministic

    d = Dispatcher(vc).use_temper(Temper([
        dedupe("images"), member_or_default("thumb", "images"),
        single_tag("status"), normalize_tags(),
    ]))
    d.add_selector("active", Selector(where="status:active", sort_key="title"))

    # ── delegation: any non-transform verb behaves exactly like vc.dispatch ────────
    assert d.dispatch("ls")["data"] == vc.dispatch("ls")["data"]
    assert d.dispatch("get alpha title")["data"] == "Alpha"
    assert d.dispatch("bogus-verb x")["ok"] is False           # error contract preserved

    # ── scry: tag-expression filter -> names ──────────────────────────────────────
    r = d.dispatch('scry "status:active"')
    assert r["ok"] and set(r["data"]) == {"alpha", "beta"}, r
    assert d.dispatch('scry "NOT status:active"')["data"] == ["gamma"]
    assert d.dispatch('scry "status:active" --limit 1')["data"] != []
    # --tag is an alias for the positional expr (parity with `ls --tag`); the C `ls --tag`
    # and the scry verb agree (the bug PM hit: --tag was silently folded into the filter)
    assert d.dispatch('scry --tag "status:active"')["data"] == r["data"]
    assert set(d.dispatch('scry --tag py')["data"]) == set(vc.dispatch('ls --tag py')["data"])
    # unknown flags are rejected, not swallowed as tags
    assert d.dispatch('scry --bogus x')["ok"] is False
    # scry never mutates
    before = vc.export_state()
    d.dispatch('scry "status:active"')
    assert vc.export_state() == before

    # ── scry --select: a registered projection runs ───────────────────────────────
    sel = d.dispatch("scry --select active")
    assert [x["spirit"]["name"] for x in sel["data"]] == ["alpha", "beta"], sel
    assert d.dispatch("scry --select missing")["ok"] is False

    # ── temper: dedupe images, default thumb, dedupe tags, collapse status ────────
    vc.dispatch("setjson alpha images '[\"a.png\",\"a.png\",\"b.png\"]'")
    t = d.dispatch("temper alpha")
    assert t["ok"] and "alpha" in t["data"], t
    rec = vc.dispatch("cat alpha")["data"]
    assert rec["content"]["images"] == ["a.png", "b.png"]       # de-duped
    assert rec["content"]["thumb"] == "a.png"                   # thumb = images[0]
    assert rec["tags"].count("py") == 1                          # tag de-duped

    # temper is undoable (it wrote through the dispatcher)
    assert vc.dispatch("undo")["ok"]

    # temper-all reaches every rune
    assert d.dispatch("temper")["ok"]

    # ── materialize: bake resolved values into owned content ──────────────────────
    m = d.dispatch("materialize beta title=Beta hits=7")
    assert m["ok"] and "beta" in m["data"], m
    rec = vc.dispatch("cat beta")["data"]
    assert rec["content"]["title"] == "Beta" and rec["content"]["hits"] == 7
    # programmatic form
    d.materialize({"gamma": {"rank": 1}})
    assert vc.dispatch("cat gamma")["data"]["content"]["rank"] == 1

    # --stamp records snapshot-id provenance of what was baked
    from voidcore import provenance
    d.dispatch("materialize beta note=hello --stamp frozen")
    assert vc.dispatch("cat beta")["data"]["content"]["frozen"] == provenance({"note": "hello"})

    # ── atomic single-frame undo: a multi-rune pass is ONE undo frame ─────────────
    h0 = len(vc.dispatch("history")["data"])
    d.materialize({"alpha": {"k": 1}, "beta": {"k": 2}, "gamma": {"k": 3}})  # 3 runes
    assert len(vc.dispatch("history")["data"]) - h0 == 1, "multi-rune pass wasn't one frame"
    assert vc.dispatch("cat alpha")["data"]["content"]["k"] == 1
    vc.dispatch("undo")                                   # ONE undo reverts all three
    for n in ("alpha", "beta", "gamma"):
        assert "k" not in vc.dispatch(f"cat {n}")["data"]["content"], f"{n} not reverted"

    # ── temper-on-write: invariants hold even for RAW edits (PM's correctness ask) ──
    d.temper_on_write(True)
    vc.dispatch("setjson alpha images '[\"x.png\",\"y.png\"]'")  # set up a valid image list
    d.dispatch("temper alpha")                                    # thumb -> x.png
    # a raw edit that BREAKS the invariant (thumb not a member of images) is auto-fixed
    res = d.dispatch("setjson alpha thumb 'not-a-member.png'")
    assert res["ok"]
    assert vc.dispatch("cat alpha")["data"]["content"]["thumb"] == "x.png", "auto-temper didn't fix raw edit"
    # a raw tag edit that duplicates is auto-deduped
    d.dispatch("tag alpha +dupe +dupe")
    assert vc.dispatch("cat alpha")["data"]["tags"].count("dupe") == 1
    # targeting: editing alpha does not temper gamma (no cross-rune surprise)
    g_before = vc.dispatch("cat gamma")["data"]
    d.dispatch("setjson alpha title 'A2'")
    assert vc.dispatch("cat gamma")["data"] == g_before
    # off by default: a fresh dispatcher does NOT auto-temper
    d2 = Dispatcher(vc).use_temper(Temper([member_or_default("thumb", "images")]))
    d2.dispatch("setjson alpha thumb 'bad-again.png'")
    assert vc.dispatch("cat alpha")["data"]["content"]["thumb"] == "bad-again.png"  # not fixed

    vc.close()
    print("DISPATCHER SEAM: OK (delegation, scry, scry --select, temper, materialize,",
          "atomic 1-frame undo, temper-on-write)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
