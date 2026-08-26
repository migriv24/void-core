"""
OKF engine CLI — the agent surface.

    python holidays/okf <command> [options]

Commands:
    ls [--tag EXPR] [--status S] [--type T]   list concepts
    get <conceptId> [--head] [--json]         show one concept (--head = header, no body)
    query <tag-expr>                          ls by a SPEC §5 tag expression
    validate                                  conformance + drift report (exit 1 on errors)
    analyze                                    graph centrality / community report

Read verbs return summaries, not dumps (/design/context-optimization.md): `ls`/`query`
are the selection surface, `get --head` is triage — title, description, tags, and the
link graph both ways — and the body is the exception you ask for, not the default.

Default bundle is the repo's `okf/`; override with --bundle DIR.

This is the *consume / validate / query* side of the OKF holiday — pure Python, no
Void Core needed (it works on any conformant bundle). Bundles are viewed in FaultSack
(the dedicated OKF study tool), not by a built-in visualizer. Producing a bundle from
a mantle, and mapping concepts into runes, are separate (later) slices.
"""
from __future__ import annotations

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bundle import load_bundle  # noqa: E402
from validate import validate as run_validate  # noqa: E402

_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_DEFAULT_BUNDLE = os.path.join(_REPO, "okf")


def _utf8_stdio() -> None:
    """Emit UTF-8 whatever the console or pipe encoding happens to be.

    `bundle.py` reads bundles as UTF-8, so a concept may legitimately hold any character —
    but `print` inherits the platform's locale codepage (cp1252 on Windows), and a single
    `⊤` or `é` then raises UnicodeEncodeError *mid-render*, replacing the concept with a
    traceback. That is worse than a display glitch: `get` is the token-efficient read path,
    so an agent that cannot read a concept here falls back to reading the whole file with a
    generic tool. `errors="replace"` is the second half of the fix and is kept even when the
    encoding cannot be changed: a stream that genuinely cannot carry a glyph should degrade
    to `?`, never abort a read.
    """
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            try:
                stream.reconfigure(errors="replace")
            except (AttributeError, ValueError):
                pass  # not a reconfigurable text stream; nothing safe to do


def _filtered(bundle, args):
    concepts = bundle.query(args.tag) if getattr(args, "tag", None) else list(bundle.concepts.values())
    if getattr(args, "status", None):
        concepts = [c for c in concepts if c.status == args.status]
    if getattr(args, "type", None):
        concepts = [c for c in concepts if c.type.lower() == args.type.lower()]
    return sorted(concepts, key=lambda c: (c.section, c.title))


def cmd_ls(bundle, args) -> int:
    for c in _filtered(bundle, args):
        print(f"  [{c.status:<9}] {c.id:<30} {c.type:<14} {c.title}")
    return 0


def cmd_get(bundle, args) -> int:
    c = bundle.get(args.id)
    if not c:
        print(f"no such concept: {args.id}", file=sys.stderr)
        return 1
    if args.json:
        print(json.dumps({
            "id": c.id, "title": c.title, "type": c.type, "status": c.status,
            "tags": c.tags, "description": c.description, "resource": c.resource,
            "timestamp": c.timestamp, "links": c.links,
            "backlinks": bundle.backlinks(c.id), "degree": bundle.degree(c.id),
        }, indent=2))
    else:
        print(f"{c.title}  ({c.type} · {c.id})")
        if c.description:
            print(f"  {c.description}")
        print(f"  tags: {', '.join(c.tags)}")
        if c.resource:
            print(f"  resource: {c.resource}")
        print(f"  links -> {c.links}")
        print(f"  linked from <- {bundle.backlinks(c.id)}")
        # `--head` stops here. Triage ("is this the page I want, and what does it link
        # to?") is answered by the header, and bodies run to tens of kilobytes — the same
        # "read verbs return summaries, not dumps" rule /design/context-optimization.md
        # states for dispatcher verbs. The body is the exception you ask for.
        if not args.head:
            print()
            print(c.text)
    return 0


def cmd_validate(bundle, args) -> int:
    rep = run_validate(bundle)
    print(rep.render())
    return 0 if rep.ok else 1


def cmd_produce(bundle, args) -> int:
    # consume into the C core, then write a (optionally filtered) bundle back out
    import voidcore_bridge
    vc = voidcore_bridge.consume(bundle)
    try:
        n = voidcore_bridge.produce(vc, args.out, where=args.where or "")
    finally:
        vc.close()
    where = f' (--where "{args.where}")' if args.where else ""
    print(f"produced {n} concepts -> {args.out}{where}")
    return 0


def cmd_analyze(bundle, args) -> int:
    sys.path.insert(0, os.path.join(_REPO, "holidays", "graph"))
    from holiday import GraphAnalyticsHoliday
    import analytics
    m = GraphAnalyticsHoliday().analyze_bundle(bundle)
    s = m["summary"]
    print(f"graph: {s['nodes']} concepts, {s['edges']} links, "
          f"{s['components']} component(s), {s['communities']} communit(y/ies)")
    print("\n  most central (betweenness - bridges the most paths):")
    for cid, v in analytics.top(m["betweenness"], 6):
        print(f"    {v:6.3f}  {cid}")
    print("\n  most authoritative (pagerank - most linked-to):")
    for cid, v in analytics.top(m["pagerank"], 6):
        print(f"    {v:6.4f}  {cid}")
    print("\n  communities:")
    groups: dict[int, list] = {}
    for cid, c in sorted(m["community"].items()):
        groups.setdefault(c, []).append(cid)
    for c, members in sorted(groups.items()):
        print(f"    [{c}] {', '.join(members)}")
    return 0


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="okf", description="OKF engine (consume/validate/serve)")
    p.add_argument("--bundle", default=_DEFAULT_BUNDLE, help="bundle directory")
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("ls"); s.add_argument("--tag"); s.add_argument("--status"); s.add_argument("--type")
    s = sub.add_parser("query"); s.add_argument("expr")
    s = sub.add_parser("get")
    s.add_argument("id")
    s.add_argument("--json", action="store_true",
                   help="machine-readable header (never includes the body)")
    s.add_argument("--head", action="store_true",
                   help="header only, no body — the triage read")
    sub.add_parser("validate")
    s = sub.add_parser("produce"); s.add_argument("out"); s.add_argument("--where")
    sub.add_parser("analyze")

    args = p.parse_args(argv)
    _utf8_stdio()
    bundle = load_bundle(args.bundle)

    if args.cmd == "ls":
        return cmd_ls(bundle, args)
    if args.cmd == "query":
        args.tag, args.status, args.type = args.expr, None, None
        return cmd_ls(bundle, args)
    if args.cmd == "get":
        return cmd_get(bundle, args)
    if args.cmd == "validate":
        return cmd_validate(bundle, args)
    if args.cmd == "produce":
        return cmd_produce(bundle, args)
    if args.cmd == "analyze":
        return cmd_analyze(bundle, args)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
