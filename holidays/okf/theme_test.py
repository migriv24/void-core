"""
theme_test.py — the palette theme resolver.

    python holidays/okf/theme_test.py
"""
from __future__ import annotations

import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bundle import load_bundle
from theme import (contrast, ensure_contrast, mix, parse_hex, resolve_theme,
                   to_hex, _COLOR_KEYS)
from validate import validate

_WHITE, _BLACK = (255, 255, 255), (0, 0, 0)


def _bundle_with_manifest(d: str, fm_lines: str):
    open(os.path.join(d, "index.md"), "w", encoding="utf-8").write(
        "---\nokf_version: 1\n---\n# Fixture\n")
    open(os.path.join(d, "app.md"), "w", encoding="utf-8").write(
        "---\ntype: Manifest\nname: Fixture\n" + fm_lines + "---\nbody\n")
    return load_bundle(d)


def main() -> int:
    # ── color math ────────────────────────────────────────────────────────────────
    assert abs(contrast(_WHITE, _BLACK) - 21.0) < 1e-6
    assert abs(contrast(_BLACK, _BLACK) - 1.0) < 1e-6
    assert parse_hex("#7c3aed") == (0x7c, 0x3a, 0xed)
    assert parse_hex("fff") == (255, 255, 255)          # 3-digit, no hash
    assert parse_hex("#ABC") == (0xaa, 0xbb, 0xcc)      # case-insensitive expand
    assert parse_hex("nope") is None and parse_hex("") is None
    assert mix(_BLACK, _WHITE, 0.5) == (128, 128, 128)

    # ── the anchor guarantee: any bg, target 4.5, always reachable ─────────────────
    for bg in ["#000000", "#ffffff", "#777777", "#7c3aed", "#f5f3ee", "#202020"]:
        rgb = parse_hex(bg)
        out, _ = ensure_contrast((128, 128, 128), rgb, 4.5)
        assert contrast(out, rgb) >= 4.5 - 1e-9, (bg, to_hex(out), contrast(out, rgb))

    # ── hostile palette: light bg, white ink, pale primary -> all pairs pass ───────
    rt = resolve_theme({"bg": "#f5f3ee", "ink": "#ffffff", "primary": "#ffe28a"})
    assert rt is not None and rt.mode == "light", rt
    assert rt.adjustments, "hostile palette should record adjustments"
    C = {k: parse_hex(v) for k, v in rt.colors.items()}
    for k in _COLOR_KEYS:
        assert k in rt.colors, f"missing color key {k}"
    for s in ("bg", "surface", "surface_strong"):
        assert contrast(C["ink"], C[s]) >= 4.5 - 1e-6, (s, rt.colors["ink"], rt.colors[s])
    for role in ("primary", "accent", "ok", "warn", "err"):
        assert contrast(C[role], C["bg"]) >= 3.0 - 1e-6, role
        assert contrast(C["on_" + role], C[role]) >= 4.5 - 1e-6, ("on_" + role, role)

    # ── compliant palette (Void Core's own) passes through byte-identical ──────────
    vc = {"primary": "#7c3aed", "accent": "#d946ef", "bg": "#0b0b12", "ink": "#e8e8f0"}
    rt = resolve_theme(vc)
    assert rt.mode == "dark" and rt.adjustments == [], rt.adjustments
    for role in ("primary", "accent", "bg", "ink"):
        assert rt.colors[role] == vc[role], (role, rt.colors[role])

    # ── idempotence: resolving the normative roles of a result changes nothing ─────
    roles = {k: rt.colors[k] for k in ("primary", "accent", "bg", "ink", "ok", "warn", "err")}
    rt2 = resolve_theme(roles)
    assert rt2.adjustments == [], rt2.adjustments
    for k in ("primary", "accent", "bg", "ink"):
        assert rt2.colors[k] == rt.colors[k]

    # ── sparse (only primary) works; empty -> None ─────────────────────────────────
    assert resolve_theme({"primary": "#3366cc"}) is not None
    assert resolve_theme({}) is None
    assert resolve_theme({"primary": "garbage"}) is None   # all unparseable -> None
    assert resolve_theme(None) is None

    # ── low-contrast dark palette: ink gets lifted ─────────────────────────────────
    rt = resolve_theme({"bg": "#202020", "ink": "#333333"})
    assert rt.mode == "dark"
    C = {k: parse_hex(v) for k, v in rt.colors.items()}
    assert contrast(C["ink"], C["bg"]) >= 4.5 - 1e-6
    assert rt.adjustments

    # ── validator: a declared illegible palette warns; a compliant one doesn't ─────
    with tempfile.TemporaryDirectory() as td:
        b = _bundle_with_manifest(td, 'palette.bg: "#f5f3ee"\npalette.ink: "#ffffff"\n'
                                      'palette.primary: "#ffe28a"\n')
        warns = [m for _, m in validate(b).warnings if "palette." in m]
        assert any("palette.ink" in m for m in warns), warns
        assert any("palette.primary" in m for m in warns), warns

    with tempfile.TemporaryDirectory() as td:
        b = _bundle_with_manifest(td, 'palette.bg: "#0b0b12"\npalette.ink: "#e8e8f0"\n'
                                      'palette.primary: "#7c3aed"\n')
        assert not [m for _, m in validate(b).warnings if "palette." in m]

    with tempfile.TemporaryDirectory() as td:                  # unparseable value warns
        b = _bundle_with_manifest(td, 'palette.primary: "not-a-color"\n')
        assert any("not a valid hex" in m for _, m in validate(b).warnings)

    print("THEME: OK (wcag math, anchor guarantee, hostile fix, compliant passthrough, "
          "idempotence, sparse/empty, validator warnings)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
