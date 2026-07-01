"""
smoke.py — end-to-end test of the MeshDB local-BaaS holiday against Void Core.

What it proves (the full loop the synthesis describes):

    Void Core dispatcher  ->  runes (in the C core)
                          ->  holiday.insert  ->  MeshDB (local graph BaaS)
                          ->  holiday.query(tagExpr)  ==  core `ls --tag tagExpr`

i.e. the holiday is a faithful external backend for the core's data, and it
resolves the *same* tag grammar (SPEC §5) the core uses — answered as native
graph queries.

Run (after `cargo build -p meshdb-server`):
    python holidays/meshdb/smoke.py
It spins up its own meshdb-server (or reuses one already on :7687), wipes it,
and tears it down at the end.
"""
from __future__ import annotations

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
sys.path.insert(0, _HERE)
sys.path.insert(0, os.path.join(_REPO, "bindings", "python"))

from meshdb_holiday import MeshDBHoliday  # noqa: E402
from voidcore import VoidCore  # noqa: E402


def _seed_core() -> VoidCore:
    """Build a small mantle of runes in the pure C core."""
    vc = VoidCore()
    vc.register_glyph({"glyph": "dialogue", "label": "Dialogue line",
                       "editor": "form", "fields": ["speaker", "text"]})
    vc.register_glyph({"glyph": "walk", "label": "Walk action",
                       "editor": "form", "fields": ["actor", "x", "y"]})
    script = [
        "mantle new castle-town",
        "rune new dialogue susie-intro",
        'set susie-intro text "Hey, Kris!"',
        "facet susie-intro who Susie",
        "tag susie-intro +chapter:2 +susie",
        "rune new dialogue ralsei-greet",
        'set ralsei-greet text "Hello!"',
        "tag ralsei-greet +chapter:2 +ralsei",
        "rune new walk kris-walk-in",
        "set kris-walk-in actor kris",
        "tag kris-walk-in +chapter:1",
    ]
    for cmd in script:
        res = vc.dispatch(cmd)
        assert res["ok"], f"core rejected: {cmd} -> {res['lines']}"
    return vc


def _sync_core_to_holiday(vc: VoidCore, holiday: MeshDBHoliday, mantle: str) -> int:
    """Push every rune of a core mantle into the holiday."""
    state = vc.export_state()
    runes = next(m["runes"] for m in state["mantles"] if m["name"] == mantle)
    for rune in runes:
        holiday.insert(rune, mantle=mantle)
    return len(runes)


def _names(runes: list[dict]) -> set[str]:
    return {r["spirit"]["name"] for r in runes}


def main() -> int:
    root = os.path.join(_HERE, ".baas-smoke")
    print("standing up local MeshDB BaaS ...")
    holiday = MeshDBHoliday.local_baas(root_dir=root)
    try:
        holiday.wipe()
        print(f"  connected: {holiday.uri}  (managed={holiday._proc is not None})")

        vc = _seed_core()
        n = _sync_core_to_holiday(vc, holiday, "castle-town")
        print(f"synced {n} runes from core mantle 'castle-town' -> holiday")

        # 1. round-trip a single rune through the graph, faithfully.
        got = holiday.get("susie-intro", mantle="castle-town")
        assert got is not None, "get(susie-intro) returned None"
        assert got["spirit"]["name"] == "susie-intro"
        assert got["content"].get("text") == "Hey, Kris!", got["content"]
        assert got["facets"]["who"] == "Susie", got["facets"]
        assert set(got["tags"]) >= {"chapter:2", "susie"}, got["tags"]
        print("get + facet/content/tag round-trip: OK")

        # 2. tag-query parity: holiday.query(expr) == core `ls --tag expr`.
        exprs = [
            "",
            "chapter:2",
            "ralsei AND chapter:2",
            "susie OR ralsei",
            "NOT chapter:2",
            "glyph:walk",
            "chapter:2 AND NOT ralsei",
        ]
        for expr in exprs:
            core_names = set(vc.dispatch(f'ls --tag "{expr}"')["data"] or [])
            holi_names = _names(holiday.query(expr, mantle="castle-town"))
            status = "OK " if core_names == holi_names else "MISMATCH"
            print(f"  [{status}] @{expr or '<all>'}: core={sorted(core_names)} "
                  f"holiday={sorted(holi_names)}")
            assert core_names == holi_names, (expr, core_names, holi_names)
        print("tag-query parity (holiday == core ls --tag): OK")

        # 3. update + delete.
        assert holiday.update("ralsei-greet", {"text": "Hi!!"}, mantle="castle-town")
        assert holiday.get("ralsei-greet", mantle="castle-town")["content"]["text"] == "Hi!!"
        assert holiday.delete("kris-walk-in", mantle="castle-town")
        assert holiday.get("kris-walk-in", mantle="castle-town") is None
        assert _names(holiday.query("", mantle="castle-town")) == {"susie-intro", "ralsei-greet"}
        print("update + delete: OK")

        # 4. describe (holiday introspection).
        desc = holiday.describe()
        print(f"describe: backend={desc['backend']} status={desc['status']} "
              f"counts={desc['counts']} tags={desc['tags']}")
        assert desc["status"] == "online"
        assert desc["counts"]["runes"] == 2

        vc.close()
        print("\nMESHDB HOLIDAY: ALL OK")
        return 0
    finally:
        holiday.close()


if __name__ == "__main__":
    sys.exit(main())
