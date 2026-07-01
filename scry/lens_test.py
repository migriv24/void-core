"""
lens_test.py — the Lens (bidirectional projection + round-trip law).

    python scry/lens_test.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                "holidays", "localjson"))

from lens import Lens  # noqa: E402
from localjson_holiday import LocalJsonHoliday, RecordSchema  # noqa: E402


def main() -> int:
    # ── a plain Lens: lossless passes, lossy is caught both ways ───────────────────
    ok = Lens(forward=lambda r: dict(r), backward=lambda r: dict(r), label="identity")
    assert ok.check([{"id": "a", "n": 1}]).ok
    lossy = Lens(forward=lambda r: dict(r),
                 backward=lambda r: {k: v for k, v in r.items() if k != "n"}, label="drops-n")
    rep = lossy.check([{"id": "a", "n": 1}])
    assert not rep.ok and any(k == "n" for _, k, _, _ in rep.mismatches)

    # ── the holiday's record↔rune mapping as ONE Lens, guarded by the law ─────────
    schema = RecordSchema(
        record_key="projects", glyph="project", id_field="id",
        tag_list_field="tags", tag_scalar_fields=("status", "year"),
        flag_fields=("featured",),
        content_fields=("title", "subtitle", "url"),
    )
    holiday = LocalJsonHoliday(path="(unused)", schema=schema)
    lens = holiday.lens()

    records = [
        {"id": "alpha", "title": "Alpha", "subtitle": "", "url": None,
         "tags": ["py", "blender"], "status": "active", "year": 2024, "featured": True},
        {"id": "beta", "title": "Beta", "subtitle": "S", "url": "x",
         "tags": ["skill:rust"], "status": "complete", "year": 2026, "featured": False},
    ]
    # forward = record→rune (persist / write-side), backward = rune→record (read-side)
    rune = lens.forward(records[0])
    assert rune["glyph"] == "project" and "status:active" in rune["tags"]
    back = lens.backward(rune)
    assert back["status"] == "active" and back["featured"] is True

    # the law: every record survives record→rune→record (the bug class PM shipped)
    rep = lens.check(records)
    assert rep.ok, rep.render()

    # a lossy schema (drops an unknown namespaced tag) is caught
    lossy_schema = RecordSchema(
        record_key="projects", glyph="project", id_field="id",
        tag_list_field=None,                      # <- no free-tag field: "py"/"skill:rust" dropped
        tag_scalar_fields=("status", "year"), flag_fields=("featured",),
        content_fields=("title",),
    )
    lossy_lens = LocalJsonHoliday(path="(unused)", schema=lossy_schema).lens()
    assert not lossy_lens.check(records).ok      # data loss detected, not shipped

    print("LENS: OK (plain lens both directions, record<->rune law, lossy schema caught)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
