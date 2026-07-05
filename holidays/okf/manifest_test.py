"""
manifest_test.py — the app-manifest reader.

    python holidays/okf/manifest_test.py
"""
from __future__ import annotations

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from manifest import read_manifest


def main() -> int:
    # ── reads Void Core's own app.md (identity + representation) ───────────────────
    here = os.path.dirname(os.path.abspath(__file__))
    okf_dir = os.path.join(here, "..", "..", "okf")
    m = read_manifest(okf_dir)
    assert m.name == "Void Core" and m.id == "voidcore", m
    assert m.version == "0.2.1" and m.status == "current"
    assert m.authors == ["migriv24"]
    assert m.icon == "rune" and m.theme == "void"
    assert m.palette.get("primary") == "#7c3aed" and m.palette.get("ink") == "#e8e8f0"
    assert m.source == "app.md", m.source

    with tempfile.TemporaryDirectory() as d:
        # ── app.md with a free representation key lands in extra ───────────────────
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "app.md"), "w", encoding="utf-8").write(
            "---\ntype: Manifest\nname: Hormiga\nversion: 2.0\n"
            "description: A community newsletter builder.\n"
            "palette.primary: \"#e11d48\"\ntheme.spatial.depth: \"3\"\n---\nbody\n")
        m = read_manifest(d)
        assert m.name == "Hormiga" and m.id == "hormiga"   # id slugified from name
        assert m.palette == {"primary": "#e11d48"}
        assert m.extra.get("theme.spatial.depth") == "3"   # free bag

        # ── no app.md -> falls back to scraping index.md (subsumes the old workaround)
        os.remove(os.path.join(d, "app.md"))
        open(os.path.join(d, "index.md"), "w", encoding="utf-8").write(
            "# Portfolio Manager\n\nManages a portfolio site's projects.\n\n# Concepts\n")
        m = read_manifest(d)
        assert m.name == "Portfolio Manager", m.name
        assert m.description == "Manages a portfolio site's projects.", m.description
        assert m.source == "index.md scrape"

        # ── empty bundle -> folder name, never crashes ────────────────────────────
        empty = os.path.join(d, "MyApp", "okf")
        os.makedirs(empty)
        m = read_manifest(empty)
        assert m.name == "MyApp", m.name   # 'okf' dir -> parent folder name

    print("APP MANIFEST: OK (app.md identity+representation, free bag, index scrape fallback, folder default)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
