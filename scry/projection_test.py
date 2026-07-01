"""
projection_test.py — Scry projection: selectors, context, materialize, invariants.

    python scry/projection_test.py
"""
from __future__ import annotations

import copy
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from projection import Context, Selector, materialize, provenance, scry  # noqa: E402

RUNES = [
    {"spirit": {"id": "1", "name": "alpha"}, "glyph": "doc",
     "tags": ["status:active", "year:2024"], "content": {"title": "Alpha"}},
    {"spirit": {"id": "2", "name": "beta"}, "glyph": "doc",
     "tags": ["status:active", "year:2026"], "content": {"title": "Beta"}},
    {"spirit": {"id": "3", "name": "gamma"}, "glyph": "doc",
     "tags": ["status:archived", "year:2025"], "content": {"title": "Gamma"}},
]


def names(rs):
    return [r["spirit"]["name"] for r in rs]


def main() -> int:
    snapshot = copy.deepcopy(RUNES)  # for the no-mutation check

    # identity is the default
    assert names(scry(RUNES)) == ["alpha", "beta", "gamma"]

    # filter by tag-expression
    assert names(scry(RUNES, where="status:active")) == ["alpha", "beta"]
    assert names(scry(RUNES, where="NOT status:active")) == ["gamma"]

    # select: project a context-parameterized view
    def title_for(rune, ctx: Context):
        t = rune["content"]["title"]
        return f"[{ctx.locale}] {t}" if ctx.locale else t
    view = scry(RUNES, where="status:active", select=title_for,
                context=Context(locale="es"))
    assert view == ["[es] Alpha", "[es] Beta"], view

    # sort + limit (sort by a content field via Selector)
    sel = Selector(where="status:active", sort_key="title", reverse=True, limit=1)
    assert names(sel.run(RUNES)) == ["beta"]

    # sort by year tag through a plain scry sort key
    by_year = scry(RUNES, sort=lambda r: r["tags"][1])  # year:* is tags[1] here
    assert names(by_year) == ["alpha", "gamma", "beta"]

    # purity: nothing above mutated the source
    assert RUNES == snapshot, "scry mutated its input!"

    # ── materialize: freeze a resolved (e.g. holiday-snapshot) projection ─────────
    resolved = {"alpha": {"hits": 42}, "beta": {"hits": 7}}  # imagine: from a snapshot
    baked = materialize(RUNES, resolved, into="content")
    assert baked[0]["content"]["hits"] == 42
    assert baked[1]["content"]["hits"] == 7
    assert "hits" not in baked[2]["content"]              # gamma untouched
    assert RUNES == snapshot, "materialize mutated its input!"   # functional
    # original title preserved alongside the baked field
    assert baked[0]["content"]["title"] == "Alpha"

    # materialize into tags
    baked_tags = materialize(RUNES, {"alpha": {"rank": 1}}, into="tags")
    assert "rank:1" in baked_tags[0]["tags"]
    assert RUNES == snapshot

    # ── provenance: stable, order-independent snapshot id ─────────────────────────
    assert provenance({"a": 1, "b": 2}) == provenance({"b": 2, "a": 1})   # key order
    assert provenance({"a": 1}) != provenance({"a": 2})                   # content matters
    assert len(provenance({"x": [1, 2, 3]})) == 16                        # short, stable id

    # materialize --stamp records what each rune froze
    stamped = materialize(RUNES, resolved, into="content", stamp="frozen")
    assert stamped[0]["content"]["frozen"] == provenance({"hits": 42})
    assert stamped[1]["content"]["frozen"] == provenance({"hits": 7})
    assert "frozen" not in stamped[2]["content"]                          # gamma untouched
    # same snapshot -> same stamp (an archive can prove what it captured)
    assert materialize(RUNES, resolved, stamp="frozen")[0]["content"]["frozen"] \
        == stamped[0]["content"]["frozen"]
    assert RUNES == snapshot

    print("SCRY PROJECTION: OK (selectors, context, sort/limit, materialize, provenance, purity)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
