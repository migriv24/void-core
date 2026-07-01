"""
voidcore_bridge.py — map OKF concepts <-> Void Core runes through the C core.

This is the OKF engine's **produce** side and the half of **consume** that turns a
bundle into a real Void Core mantle (the other half — pure-Python querying — lives in
`bundle.py` and needs no core). It realizes the glossary mapping in actual code:

    Concept            <-> rune (glyph "okf-concept")
    Concept ID         <-> spirit.name
    type               <-> a `type:<value>` tag
    description/resource/timestamp <-> facets what/where/when
    tags               <-> tags
    body (markdown)    <-> content.body (opaque)
    link               <-> mantle layout.edge {from, to, relation:"links"}
    Bundle             <-> mantle

`consume` hydrates a state document and loads it through `vc_create` (bulk import),
then the dispatcher operates on it. `produce` reads `export_state` back out and writes
a conformant bundle, filtered by an optional tag expression (the library projection).
"""
from __future__ import annotations

import os
import re
import secrets
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))
sys.path.insert(0, _HERE)
sys.path.insert(0, os.path.join(_REPO, "bindings", "python"))

from bundle import Bundle, load_bundle  # noqa: E402
from voidcore import VoidCore  # noqa: E402

OKF_GLYPH = {"glyph": "okf-concept", "label": "OKF concept", "editor": "markdown",
             "fields": ["body", "title", "description", "resource", "timestamp", "okf_type"]}
_FACETS = ("who", "what", "when", "where", "why", "how")
MANTLE = "okf"


def _mint() -> str:
    return f"rune_{secrets.token_hex(6)}"


def consume(bundle: Bundle, mantle: str = MANTLE) -> VoidCore:
    """Bundle -> a Void Core manager holding one mantle of `okf-concept` runes."""
    runes = []
    for c in bundle.concepts.values():
        facets = {k: "" for k in _FACETS}
        facets["what"], facets["where"], facets["when"] = c.description, c.resource, c.timestamp
        runes.append({
            "spirit": {"id": _mint(), "name": c.id},
            "glyph": "okf-concept",
            "facets": facets,
            "tags": list(c.tags) + [f"type:{c.type}"],
            "content": {"body": c.body, "title": c.title, "description": c.description,
                        "resource": c.resource, "timestamp": c.timestamp, "okf_type": c.type},
            "placement": None,
            "relations": [],
        })
    edges = [{"from": a, "to": b, "relation": "links"} for a, b in bundle.edges]
    state = {
        "version": 1, "domains": {}, "mantles": [{
            "id": f"mantle_{secrets.token_hex(6)}", "name": mantle, "domain": None,
            "runes": runes, "tags": {}, "layout": {"edges": edges}, "rules": [],
        }],
        "bindings": [], "scripts": {}, "config": {},
        "active": {"mantle": mantle, "domain": None},
    }
    vc = VoidCore(state=state)
    vc.register_glyph(OKF_GLYPH)
    return vc


def produce(vc: VoidCore, out_dir: str, *, where: str = "", mantle: str = MANTLE) -> int:
    """Void Core mantle -> a conformant OKF bundle on disk. `where` is a tag filter
    (e.g. the library projection). Returns the number of concepts written."""
    vc.dispatch(f"use {mantle}")
    names = vc.dispatch(f'ls --tag "{where}"' if where else "ls")["data"] or []
    state = vc.export_state()
    by_name = {r["spirit"]["name"]: r
               for m in state["mantles"] if m["name"] == mantle for r in m["runes"]}
    written = 0
    for name in names:
        r = by_name[name]
        content = r.get("content", {})
        okf_type = content.get("okf_type") or _type_from_tags(r["tags"]) or "Concept"
        tags = [t for t in r["tags"] if not t.startswith("type:")]
        path = os.path.join(out_dir, *name.split("/")) + ".md"
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(_frontmatter(okf_type, content, tags))
            f.write(content.get("body", ""))
        written += 1
    return written


def _type_from_tags(tags: list[str]) -> str:
    for t in tags:
        if t.startswith("type:"):
            return t.split(":", 1)[1]
    return ""


def _frontmatter(okf_type: str, content: dict, tags: list[str]) -> str:
    lines = ["---", f"type: {okf_type}"]
    if content.get("title"):
        lines.append(f"title: {content['title']}")
    if content.get("description"):
        lines.append(f"description: {content['description']}")
    if content.get("resource"):
        lines.append(f"resource: {content['resource']}")
    if tags:
        lines.append("tags: [" + ", ".join(tags) + "]")
    if content.get("timestamp"):
        lines.append(f"timestamp: {content['timestamp']}")
    lines.append("---\n\n")
    return "\n".join(lines)


# ── round-trip self-test ─────────────────────────────────────────────────────────
def _round_trip(bundle_dir: str) -> int:
    import tempfile
    src = load_bundle(bundle_dir)
    vc = consume(src)

    # the core's tag engine should answer the same as the OKF engine's evaluator
    core_current = set(vc.dispatch('ls --tag "status:current AND audience:library"')["data"] or [])
    okf_current = {c.id for c in src.query("status:current AND audience:library")}
    assert core_current == okf_current, (core_current ^ okf_current)
    print(f"tag-parity (core ls --tag == OKF query): OK  ({len(core_current)} library concepts)")

    out = os.path.join(tempfile.mkdtemp(prefix="okf_rt_"), "bundle")
    n = produce(vc, out)
    rt = load_bundle(out)

    assert set(rt.concepts) == set(src.concepts), set(rt.concepts) ^ set(src.concepts)
    assert set(rt.edges) == set(src.edges), set(rt.edges) ^ set(src.edges)
    for cid, c in src.concepts.items():
        d = rt.concepts[cid]
        assert d.type == c.type, (cid, d.type, c.type)
        assert set(d.tags) == set(c.tags), (cid, set(d.tags) ^ set(c.tags))
        assert d.body.strip() == c.body.strip(), cid
    print(f"round-trip bundle->core->bundle: OK  ({n} concepts, {len(rt.edges)} links identical)")

    # produce only the library projection
    lib = os.path.join(os.path.dirname(out), "lib")
    k = produce(vc, lib, where="status:current AND audience:library")
    libbundle = load_bundle(lib)
    assert set(libbundle.concepts) == okf_current
    assert "roadmap" not in libbundle.concepts
    print(f"library projection produced: OK  ({k} concepts, roadmap excluded)")
    vc.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(_round_trip(os.path.join(_REPO, "okf")))
