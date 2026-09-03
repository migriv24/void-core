"""
glyph_declaration_test.py — a bundle must carry its runes AND their meaning.

Void Hormiga, 2026-09-03. Descriptors used to live only on the manager: the
built-ins plus whatever the host registered at boot. The state document's
top-level keys were `_baseline, active, bindings, config, domains, mantles,
scripts, version` — no glyphs. So a `.miga` carried runes but not their meaning:
open the bundle on a machine whose host did not register the same descriptors and
the content survives verbatim in the document while the projection has no fields.
The data is *present and unreachable*.

That is the one property a conformance case cannot show, because a case runs
inside one manager. This test exports a document and reopens it on a manager that
registered nothing, which is exactly the shape of handing a bundle to somebody.

    python bindings/python/glyph_declaration_test.py
"""
from __future__ import annotations

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from voidcore import VoidCore  # noqa: E402

FAIL: list[str] = []


def check(cond: bool, msg: str) -> None:
    if not cond:
        FAIL.append(msg)
    print(("  ok   " if cond else "  FAIL ") + msg)


def q(obj) -> str:
    """One dispatcher argument holding a JSON descriptor (SPEC §6.1)."""
    return "'" + json.dumps(obj, separators=(",", ":")) + "'"


def main() -> int:
    # ── an application declares its own record type and uses it ─────────────
    a = VoidCore()
    a.dispatch("mantle new roster")
    a.dispatch("glyph declare " + q({
        "glyph": "contact",
        "label": "Contact",
        "kind": "entity",
        "fields": ["name", "email", "org"],
        "presentations": {"canvas": {"color": "#2b7"}, "email": {"template": "row"}},
    }))
    a.dispatch("glyph declare " + q({
        "glyph": "stat", "label": "Stat", "kind": "measure", "fields": ["note"],
    }))
    a.dispatch("rune new contact ada")
    a.dispatch("set ada email ada@example.org")
    a.dispatch("rune new stat reach")
    a.dispatch("measure reach --level ratio --unit people")
    a.dispatch("link ada reach --weight 4200")

    doc = a.export_state()
    check("glyphs" in doc, "the exported document has a `glyphs` key (SPEC §2)")
    check("contact" in doc.get("glyphs", {}),
          "the declaration itself is in the document, not only on the manager")

    # ── the bundle is handed to somebody: a manager that registered nothing ──
    b = VoidCore(doc)
    b.dispatch("use roster")

    gl = b.dispatch("glyphs contact")
    check(gl["ok"], "the receiving host can read the declared descriptor")
    check(gl["data"]["fields"] == ["name", "email", "org"],
          "the schema arrived: fields are readable without the sending host's code")
    check(gl["data"]["kind"] == "entity", "the kind arrived (defaults resolved)")
    check(gl["data"]["source"] == "document",
          "`source` says the descriptor came from the document, not this host")
    check(gl["data"]["presentations"]["email"]["template"] == "row",
          "presentations travel with the schema and stay uninterpreted")

    content = b.dispatch("get ada")["data"]
    check(content.get("email") == "ada@example.org",
          "the content was always there; now the schema explaining it is too")

    vals = b.dispatch("values")["data"]
    check(vals == [{"of": "ada", "measure": "reach", "value": 4200,
                    "unit": "people", "level": "ratio", "relation": ""}],
          "the attribute assertion reads with its unit on the receiving host")

    check(b.dispatch("validate")["ok"],
          "`validate` accepts a document-declared glyph (either registry answers)")
    check(b.dispatch("ls --kind measure")["data"] == ["reach"],
          "kinds are queryable on the receiving host")

    # ── and the failure that motivated it, still true where it should be ────
    # A rune whose glyph was only ever REGISTERED does not survive the trip: the
    # host registry is host config and is deliberately not exported (§3.3).
    c = VoidCore()
    c.register_glyph({"glyph": "hostonly", "label": "Host only", "fields": ["x"]})
    c.dispatch("mantle new h")
    c.dispatch("rune new hostonly thing")
    d = VoidCore(c.export_state())
    d.dispatch("use h")
    check(not d.dispatch("glyphs hostonly")["ok"],
          "a merely-registered glyph is still host config, and still does not travel")
    check(not d.dispatch("validate")["ok"],
          "...and `validate` names it, which is how a host is told to declare it")

    # ── the declaration is undoable, because a schema is authored content ────
    e = VoidCore()
    e.dispatch("glyph declare " + q({"glyph": "temp", "label": "T"}))
    check(e.dispatch("glyphs temp")["ok"], "declared")
    e.dispatch("undo")
    check(not e.dispatch("glyphs temp")["ok"], "undo takes a declaration back")
    e.dispatch("redo")
    check(e.dispatch("glyphs temp")["ok"], "redo restores it")

    if FAIL:
        print("\nGLYPH DECLARATIONS: FAIL")
        return 1
    print("\nGLYPH DECLARATIONS: OK (schema travels with the document; "
          "host registrations still do not)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
