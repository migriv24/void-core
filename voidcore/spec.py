"""
voidcore/spec.py — data-authored transformation specs (JSON ⇄ tested layer objects).

The transformation layers are configured by *code* (a `Temper([...])`, a `Selector(...)`).
This module lets the same configuration be expressed as **JSON data**, so it can live in
the state document (a mantle's / manager's `config`), be authored or edited by a user or
another app, be serialized and shared — and then **compile** to the exact same tested
objects. Code stays the reference; data is a serialization of it.

This is the foundation the app agents converged on (notes/handoff-transform-layers.md):
- DAW wants routing/normalization rules to be user-editable **data**, not code.
- Portfolio Manager wants its record↔rune mapping to be one declared spec, not triplicated.
- The reserved `reduce` dispatcher verb needs rules authored as mantle data.

Spec formats
------------
Temper spec — a list of rule objects (each has a `rule` name + that rule's args):

    [{"rule": "dedupe", "field": "images"},
     {"rule": "member_or_default", "target": "thumb", "source": "images"},
     {"rule": "default_tag", "namespace": "status", "value": "complete"},
     {"rule": "single_tag", "namespace": "status"},
     {"rule": "normalize_tags"}]

Selector spec — a dict (the data-expressible subset of `Selector`; a callable `select`
stays code):

    {"where": "status:active", "sort": "title", "reverse": false, "limit": 10}
"""
from __future__ import annotations

from projection import Selector
from reduce import Reducer, annihilate, commute, patch
from temper import (
    Temper, dedupe, default_content, default_tag, member_or_default,
    normalize_tags, single_tag,
)

# Each entry: rule name -> (constructor, required arg keys). The constructor reads the
# spec dict; required keys are checked first so errors name the missing field.
_TEMPER_RULES = {
    "dedupe": (lambda s: dedupe(s["field"]), ("field",)),
    "member_or_default": (
        lambda s: member_or_default(s["target"], s["source"],
                                    index=s.get("index", 0), empty=s.get("empty")),
        ("target", "source")),
    "default_content": (lambda s: default_content(s["field"], s["value"]), ("field", "value")),
    "default_tag": (lambda s: default_tag(s["namespace"], s["value"]), ("namespace", "value")),
    "single_tag": (lambda s: single_tag(s["namespace"]), ("namespace",)),
    "normalize_tags": (lambda s: normalize_tags(sort=bool(s.get("sort", False))), ()),
}


def temper_from_spec(spec: list[dict]) -> Temper:
    """Compile a Temper rule-list spec into a `Temper` pass. Raises `ValueError` on an
    unknown rule or a missing required argument (with the offending index named)."""
    if not isinstance(spec, list):
        raise ValueError("temper spec must be a list of rule objects")
    rules = []
    for i, item in enumerate(spec):
        name = item.get("rule")
        entry = _TEMPER_RULES.get(name)
        if entry is None:
            raise ValueError(f"temper spec [{i}]: unknown rule {name!r} "
                             f"(known: {', '.join(sorted(_TEMPER_RULES))})")
        ctor, required = entry
        missing = [k for k in required if k not in item]
        if missing:
            raise ValueError(f"temper spec [{i}] ({name}): missing {', '.join(missing)}")
        rules.append(ctor(item))
    return Temper(rules)


def selector_from_spec(spec: dict) -> Selector:
    """Compile a selector spec (`where`/`sort`/`reverse`/`limit`) into a `Selector`."""
    if not isinstance(spec, dict):
        raise ValueError("selector spec must be an object")
    return Selector(where=spec.get("where"), sort_key=spec.get("sort"),
                    reverse=bool(spec.get("reverse", False)), limit=spec.get("limit"))


def temper_rule_names() -> list[str]:
    """The temper rules expressible as data (for editors / validation / docs)."""
    return sorted(_TEMPER_RULES)


# Reduce rule kinds expressible as data. `expand` needs a custom build fn, so it stays
# code-registered (not data) — these are the two confluent interaction-combinator rules.
_REDUCE_RULES = {"annihilate": annihilate, "commute": commute, "patch": patch}


def reducer_from_spec(spec: dict) -> tuple[Reducer, dict]:
    """Compile a reducer spec into `(Reducer, signatures)` — the data-authored form of an
    interaction-net rewriter (the mantle authoring its own rules + port arities). Returns the
    `signatures` (glyph → aux-port count) alongside, since `to_net` needs them.

        {"signatures": {"con": 2, "dup": 2, "era": 0},
         "rules": [{"glyphs": ["con", "con"], "rule": "annihilate", "swap": true},
                   {"glyphs": ["dup", "dup"], "rule": "annihilate"},
                   {"glyphs": ["con", "dup"], "rule": "commute"}]}

    `swap` (annihilate only) selects the index-reversed flavor — γγ links mirrored
    (x_i ≡ y_{n+1-i}), δδ index-straight; the asymmetry is load-bearing in Lafont's
    calculus. The conflict guard still applies (≤1 rule per glyph pair → `Reducer.rule`
    raises on a duplicate). Raises `ValueError` on an unknown rule kind, a malformed
    glyph pair, or `swap` on a non-annihilate rule."""
    if not isinstance(spec, dict):
        raise ValueError("reducer spec must be an object")
    signatures = {str(k): int(v) for k, v in (spec.get("signatures") or {}).items()}
    reducer = Reducer()
    for i, item in enumerate(spec.get("rules") or []):
        glyphs = item.get("glyphs")
        if not (isinstance(glyphs, (list, tuple)) and len(glyphs) == 2):
            raise ValueError(f"reducer spec rule [{i}]: `glyphs` must be a [a, b] pair")
        kind = item.get("rule")
        if kind not in _REDUCE_RULES:
            raise ValueError(f"reducer spec rule [{i}]: unknown rule {kind!r} "
                             f"(known: {', '.join(sorted(_REDUCE_RULES))})")
        if "swap" in item and kind != "annihilate":
            raise ValueError(f"reducer spec rule [{i}] ({kind}): `swap` only applies "
                             f"to annihilate")
        for k in ("keep", "set", "copy"):
            if k in item and kind != "patch":
                raise ValueError(f"reducer spec rule [{i}] ({kind}): `{k}` only applies "
                                 f"to patch")
        if kind == "patch":
            fn = _patch_from_item(i, glyphs, item)
        elif kind == "annihilate":
            fn = annihilate(swap=bool(item.get("swap", False)))
        else:
            fn = commute()
        reducer.rule(glyphs[0], glyphs[1], fn)
    return reducer, signatures


def _patch_from_item(i: int, glyphs, item: dict):
    """Compile one `patch` rule, validating at COMPILE time what would otherwise be a
    surprise at fire time — which matters more here than for the structural rules, because
    a patch that names the wrong survivor still reduces, just wrongly."""
    ga, gb = str(glyphs[0]), str(glyphs[1])
    if ga == gb:
        raise ValueError(
            f"reducer spec rule [{i}] (patch): needs distinct glyphs, got {ga!r} twice. "
            f"On an unordered same-glyph pair there is no non-arbitrary answer to which "
            f"side survives, and an arbitrary tiebreak is schedule-dependence.")
    keep = item.get("keep")
    if keep not in (ga, gb):
        raise ValueError(
            f"reducer spec rule [{i}] (patch): `keep` must name one of the pair's glyphs "
            f"({ga!r} or {gb!r}), got {keep!r}")
    set_fields, copy_fields = item.get("set"), item.get("copy")
    for name, m in (("set", set_fields), ("copy", copy_fields)):
        if m is not None and not isinstance(m, dict):
            raise ValueError(f"reducer spec rule [{i}] (patch): `{name}` must be an object")
    if copy_fields and not all(isinstance(v, str) for v in copy_fields.values()):
        raise ValueError(f"reducer spec rule [{i}] (patch): `copy` values name fields on "
                         f"the consumed agent, so each must be a string")
    if not set_fields and not copy_fields:
        raise ValueError(
            f"reducer spec rule [{i}] (patch): writes nothing — give it `set`, `copy`, or "
            f"both. A pair that meets and changes no content is `annihilate`.")
    return patch(keep=keep, set_fields=set_fields, copy_fields=copy_fields)


def reduce_rule_names() -> list[str]:
    """The reduce rule kinds expressible as data (for editors / validation / docs)."""
    return sorted(_REDUCE_RULES)


def boxes_from_spec(spec: dict) -> dict:
    """Compile the `boxes` half of a reducer spec — the glyphs that mean "a rune of this
    glyph *is* that mantle", so the adapter splices the mantle's net in at the rune's
    ports (`reduce/box.py`).

        {"signatures": {"dye": 1, "cloth": 2},
         "boxes": {"player": {"mantle": "player",
                              "interface": ["skin:0", "shirt:1", "voice:0"]}},
         "rules": [...]}

    `interface` orders the sub-net's free ports; entry 0 becomes the box's principal.
    It is optional (canonical order without it) and, when given, must be a permutation of
    the free ports the sub-net actually has — checked at composition time, since it is a
    fact about the mantle rather than about this spec. Returns `{glyph: Box}`; an absent
    or empty `boxes` gives `{}`, which is the flat adapter unchanged.

    Kept separate from `reducer_from_spec` rather than added to its return tuple: hosts
    (and `conformance/reduce/run.py`) already unpack that as `(reducer, signatures)`, and
    boxing is opt-in enough not to be worth breaking every caller over."""
    from box import Box
    if not isinstance(spec, dict):
        raise ValueError("reducer spec must be an object")
    out = {}
    for glyph, item in (spec.get("boxes") or {}).items():
        if not isinstance(item, dict):
            raise ValueError(f"box {glyph!r}: must be an object "
                             f"{{\"mantle\": ..., \"interface\": [...]}}")
        mantle = item.get("mantle")
        if not isinstance(mantle, str) or not mantle:
            raise ValueError(f"box {glyph!r}: `mantle` must be a non-empty string")
        iface = item.get("interface")
        if iface is not None:
            if not isinstance(iface, (list, tuple)) or not all(isinstance(s, str) for s in iface):
                raise ValueError(f"box {glyph!r}: `interface` must be a list of "
                                 f"\"<agent>:<port>\" strings")
            iface = tuple(iface)
        out[str(glyph)] = Box(mantle=mantle, interface=iface)
    return out
