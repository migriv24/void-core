"""
bilingual_example.py — answering Hormiga's question: context-aware variant selection.

Hormiga's EN/ES newsletter pick is "dedupe by pair, prefer the locale-matched variant,
fall back to neutral." That is NOT a Temper rule — Temper is context-blind owned-state
normalization. It depends on Context.locale, so it lives in **Scry**, as a projection:

    pick = dedupe_by(scry(runes, where=...), key=pair_of, prefer=by_locale, context=ctx)

`dedupe_by` groups by `key` and keeps, per group, the rune minimizing `prefer(rune, ctx)`.
Same (runes, context) ⇒ identical pick (pure), so live preview == final render == archive.

    python scry/bilingual_example.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from projection import Context, dedupe_by, scry  # noqa: E402


def rune(name, tags, title):
    return {"spirit": {"id": name, "name": name}, "glyph": "block",
            "tags": tags, "content": {"title": title}}


# A content library: two paired EN/ES blocks, one EN-only block, one neutral (no lang).
LIBRARY = [
    rune("welcome_en", ["block", "pair:1", "lang:en"], "Welcome"),
    rune("welcome_es", ["block", "pair:1", "lang:es"], "Bienvenido"),
    rune("jobs_en",    ["block", "pair:2", "lang:en"], "Jobs"),         # no ES variant
    rune("logo",       ["block", "pair:3"],            "Logo"),         # neutral, no lang
]


def pair_of(r):
    return next((t for t in r["tags"] if t.startswith("pair:")), r["spirit"]["name"])


def by_locale(r, ctx: Context):
    """Lower is better: an exact locale match (0) beats neutral (1) beats other-lang (2)."""
    langs = [t.split(":", 1)[1] for t in r["tags"] if t.startswith("lang:")]
    if not langs:
        return 1                          # neutral — an acceptable fallback
    return 0 if ctx.locale in langs else 2


def render(locale: str):
    ctx = Context(locale=locale)
    shown = scry(LIBRARY, where="block")          # the active set (could be any tag filter)
    picked = dedupe_by(shown, pair_of, prefer=by_locale, context=ctx)
    return [(pair_of(r), r["content"]["title"]) for r in picked]


def main() -> int:
    es = render("es")
    en = render("en")
    print("locale=es ->", es)
    print("locale=en ->", en)

    # ES issue: pair1 -> Spanish; pair2 -> falls back to the only (EN) variant; logo neutral
    assert es == [("pair:1", "Bienvenido"), ("pair:2", "Jobs"), ("pair:3", "Logo")], es
    # EN issue: pair1 -> English; rest unchanged
    assert en == [("pair:1", "Welcome"), ("pair:2", "Jobs"), ("pair:3", "Logo")], en
    # purity: same inputs -> identical pick
    assert render("es") == es

    print("BILINGUAL (context-aware dedupe via Scry): OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
