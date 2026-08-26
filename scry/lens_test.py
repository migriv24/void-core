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

from lens import Lens, pipeline  # noqa: E402
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

    # ── compose: the pivot rule, and the normalizer bug it hides ─────────────────
    # Void Hormiga's correction (2026-08-17): the composite normalizer must be
    # `self.normalize or other.normalize`. Their first proposal (`self.normalize`)
    # produced a composite that compared whitespace the inner lens knew to collapse,
    # and the law failed on data that round-trips perfectly.
    collapse = lambda d: {**d, "text": " ".join(str(d.get("text", "")).split())}
    normalizing = Lens(forward=lambda d: {**d, "text": f"  {d['text']}  "},
                       backward=lambda d: dict(d),
                       normalize=collapse, label="pads-text")
    trivial = Lens(forward=lambda d: dict(d), backward=lambda d: dict(d), label="trivial")

    assert normalizing.check([{"id": "a", "text": "one two"}]).ok      # holds alone
    composite = trivial.compose(normalizing)                          # no-opinion o opinion
    assert composite.normalize is collapse, "composite dropped its argument's normalizer"
    assert composite.check([{"id": "a", "text": "one two"}]).ok, composite.check(
        [{"id": "a", "text": "one two"}]).render()
    # ...and the other order, so the fallback is not accidentally one-sided
    assert normalizing.compose(trivial).normalize is collapse

    # an explicit normalizer still wins over the fallback
    assert trivial.compose(normalizing, normalize=None).normalize is collapse
    mine = lambda d: d
    assert trivial.compose(normalizing, normalize=mine).normalize is mine

    # composition is genuinely the pivot: A -> pivot -> C, lossless because its legs are
    to_pivot = Lens(forward=lambda r: {"v": r["a"]}, backward=lambda p: {"a": p["v"]},
                    label="A->pivot")
    to_c = Lens(forward=lambda p: {"c": p["v"]}, backward=lambda q: {"v": q["c"]},
                label="pivot->C")
    chain = to_pivot.compose(to_c)
    assert chain.forward({"a": 7}) == {"c": 7} and chain.check([{"a": 7}, {"a": 8}]).ok
    assert chain.label == "A->pivot -> pivot->C"

    # a lossy leg makes the composite lossy - the law is inherited, not assumed
    drops = Lens(forward=lambda p: {"c": p["v"]}, backward=lambda q: {}, label="lossy-leg")
    assert not to_pivot.compose(drops).check([{"a": 7}]).ok

    # ── identity + pipeline: the monoid, and why the empty chain matters ──────────
    ident = Lens.identity()
    assert ident.check([{"a": 1}]).ok and ident.normalize is None
    assert ident.compose(to_pivot).forward({"a": 3}) == to_pivot.forward({"a": 3})
    assert to_pivot.compose(ident).forward({"a": 3}) == to_pivot.forward({"a": 3})
    assert pipeline().forward({"a": 1}) == {"a": 1}          # empty chain is well-defined
    assert pipeline(to_pivot, to_c).forward({"a": 9}) == {"c": 9}
    assert pipeline(to_pivot, to_c).check([{"a": 9}]).ok

    # ── check records a raised exception as a failure, and keeps going ────────────
    def explodes(r):
        if r["a"] == 2:
            raise TypeError("no")
        return {"v": r["a"]}
    thrower = Lens(forward=explodes, backward=lambda p: {"a": p["v"]}, label="throws")
    rep = thrower.check([{"a": 1}, {"a": 2}, {"a": 3}])
    assert rep.checked == 3, rep.checked                     # did not stop at the throw
    assert len(rep.mismatches) == 1 and "TypeError" in str(rep.mismatches[0][3]), rep.render()

    print("LENS: OK (plain lens both directions, record<->rune law, lossy schema caught,",
          "compose + normalizer fallback, identity/pipeline monoid, exception-as-failure)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
