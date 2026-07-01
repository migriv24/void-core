"""
spec_test.py — data-authored transformation specs compile to the tested layer objects,
ride in the state document, and reload to drive the dispatcher verbs.

    python voidcore/spec_test.py
"""
from __future__ import annotations

import json
import sys

from voidcore import (
    Dispatcher, Temper, VoidCore,
    dedupe, member_or_default, normalize_tags, selector_from_spec, single_tag,
    temper_from_spec, temper_rule_names,
)

TEMPER_SPEC = [
    {"rule": "dedupe", "field": "images"},
    {"rule": "member_or_default", "target": "thumb", "source": "images"},
    {"rule": "single_tag", "namespace": "status"},
    {"rule": "normalize_tags"},
]
SELECTOR_SPEC = {"where": "status:active", "sort": "title"}


def sample_rune(name, tags, content):
    return {"spirit": {"id": name, "name": name}, "glyph": "shot",
            "tags": list(tags), "content": dict(content)}


def main() -> int:
    # ── compile parity: spec-built Temper == code-built Temper ────────────────────
    coded = Temper([dedupe("images"), member_or_default("thumb", "images"),
                    single_tag("status"), normalize_tags()])
    speced = temper_from_spec(TEMPER_SPEC)
    r = sample_rune("a", ["status:x", "status:y", "dup", "dup"],
                    {"images": ["p.png", "p.png", "q.png"], "thumb": "gone.png"})
    assert coded.rune(r) == speced.rune(r), "spec-built Temper diverged from code-built"
    # idempotence still holds for the compiled pass
    once = speced.rune(r)
    assert speced.rune(once) == once

    # selector spec compiles
    sel = selector_from_spec(SELECTOR_SPEC)
    assert sel.where == "status:active" and sel.sort_key == "title"

    # ── full state-document round-trip: specs in config drive the verbs ───────────
    state = {
        "version": 1, "domains": {},
        "mantles": [{"id": "m", "name": "main", "domain": None, "runes": [],
                     "tags": {}, "layout": {"edges": []}, "rules": []}],
        "bindings": [], "scripts": {},
        "config": {"transform": {"temper": TEMPER_SPEC,
                                 "selectors": {"active": SELECTOR_SPEC}}},
        "active": {"mantle": "main", "domain": None},
    }
    # reopen from exported state to prove specs persist with the data they govern
    vc = VoidCore(state=state)
    vc2 = VoidCore(state=vc.export_state())
    vc.close()
    vc2.register_glyph({"glyph": "shot", "label": "Shot", "editor": "form",
                        "fields": ["title", "images", "thumb"]})
    for n in ("alpha", "beta"):
        vc2.dispatch(f"rune new shot {n}")
    vc2.dispatch("setjson alpha title 'Alpha'")
    vc2.dispatch("setjson beta title 'Bravo'")
    vc2.dispatch("tag alpha +status:active")
    vc2.dispatch("tag beta +status:archived")

    # no code-registered rules — load them from the state document
    d = Dispatcher(vc2).load_from_config().temper_on_write(True)

    # the data-authored selector drives `scry --select`
    sel_rows = d.dispatch("scry --select active")
    assert [x["spirit"]["name"] for x in sel_rows["data"]] == ["alpha"], sel_rows

    # the data-authored Temper drives temper-on-write: a raw bad edit is auto-repaired
    vc2.dispatch("setjson alpha images '[\"x.png\",\"x.png\"]'")
    d.dispatch("setjson alpha thumb 'not-a-member.png'")
    rec = vc2.dispatch("cat alpha")["data"]
    assert rec["content"]["images"] == ["x.png"], rec        # deduped by spec
    assert rec["content"]["thumb"] == "x.png", rec           # member_or_default by spec
    vc2.close()

    # ── validation: unknown rule + missing arg are clear errors ───────────────────
    for bad in ([{"rule": "nope"}], [{"rule": "dedupe"}], [{"rule": "member_or_default", "target": "t"}]):
        try:
            temper_from_spec(bad)
            assert False, f"expected ValueError for {bad}"
        except ValueError:
            pass
    assert "member_or_default" in temper_rule_names()

    print("SPEC: OK (compile parity, selector spec, state-doc round-trip drives verbs, validation)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
