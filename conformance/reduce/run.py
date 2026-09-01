"""
run.py — run the language-neutral Reduce executor conformance cases (see README.md).

Each case in cases/*.json is a pure data fixture: a reducer spec (the same
`config.transform.reduce` form the state document carries), an input mantle, options,
and the expected outcome — either the portable canonical form of the normal-form net,
or an abstract error kind. This runner validates the Python reference implementation
(`reduce/`) against the cases; a host implementing the executor in another language
ports THIS file (~100 lines) and runs the same JSON.

    python conformance/reduce/run.py            # run all cases
    python conformance/reduce/run.py --regen    # regenerate `expect` from the reference

`--regen` is for authoring new cases: it writes what the reference produces back into
the fixture. Regenerated expectations must be eyeballed before committing — the
reference is the oracle, but a golden file is only as good as its review.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, ROOT)  # the repo-root `voidcore` package wires the layer paths

from voidcore import (  # noqa: E402
    BoxError, Net, NetError, ReduceError, boxes_from_spec, compose, fresh_id_minter,
    reducer_from_spec, to_net,
)


def _key(x) -> str:
    """The one sort key: canonical JSON text (sorted keys, no whitespace)."""
    return json.dumps(x, sort_keys=True, separators=(",", ":"))


def canonical(net: Net) -> dict:
    """The portable canonical form (README.md §4): an id-independent, JSON-comparable
    fingerprint of a net — agents as a sorted [glyph, content, sorted-tags] multiset,
    each undirected wire once as sorted [glyph, port] endpoints, free ports likewise."""
    agents = sorted(([a.glyph, a.content, sorted(a.tags)]
                     for a in net.agents.values()), key=_key)

    def endp(p):
        return [net.agents[p[0]].glyph, p[1]]

    wires = sorted((sorted([endp(p), endp(q)], key=_key)
                    for p, q in net.link.items() if p <= q), key=_key)
    free = sorted((endp(p) for p in net.free_ports()), key=_key)
    return {"agents": agents, "wires": wires, "free": free}


def mint(case: dict) -> dict:
    """Run a MINTER case (README.md §2): fresh-id vectors, no reduction. Each entry is a
    redex key — the two glyphs, the two parent ids, an ordinal — and the expectation is
    the id it must produce. Split out from the reduction cases on Void Unity's ask
    (2026-08-28): case 15 pins the derived-id property, but only through a rewrite, so a
    failure there does not say whether the digest, the ordinal, or the pair ordering is
    the part that is wrong. These vectors say it directly."""
    ids = []
    for i, v in enumerate(case["minter"]):
        ga, gb = v["glyphs"]
        pa, pb = v["parents"]
        ordinal = int(v["ordinal"])
        if ordinal < 1:
            # Said rather than assumed: the ordinal counts minter CALLS within one
            # rewrite, so it is 1-based and 0 is a malformed vector, not "no id". This
            # used to fall out of the loop with the result variable unbound and raise a
            # NameError, which tells a porter nothing (Void Unity, 2026-08-29).
            raise ValueError(f"minter vector [{i}]: ordinal {ordinal} is not 1-based; "
                             f"the ordinal counts minter calls within one rewrite, "
                             f"starting at 1")
        fresh = fresh_id_minter(ga, gb, pa, pb)
        for _ in range(ordinal):             # the ordinal counts calls within one rewrite
            got = fresh()
        ids.append(got)
    return {"ids": ids}


def execute(case: dict) -> dict:
    """Run one case against the reference; return {"canonical": ...} or {"error": kind}.
    Error kinds (README.md §5): adapter-ports | locality | termination-guard."""
    if "minter" in case:
        return mint(case)
    try:
        reducer, sigs = reducer_from_spec(case["spec"])
        boxes = boxes_from_spec(case["spec"])
        # `mantles` (README §7) makes the case's `input` composable: a rune whose glyph is
        # a declared box is spliced in as that mantle's net. With no boxes this is exactly
        # `to_net`, so every pre-existing case runs down the identical path.
        net = compose(case["input"], sigs, boxes=boxes,
                      mantles={m["name"]: m for m in (case.get("mantles") or [])})
        strict = bool(case.get("strict_locality", False))
        out = reducer.reduce(net,
                             max_steps=case.get("max_steps", 100_000),
                             opaque=set(case.get("opaque") or ()),
                             strict_locality=strict)
        result = {"canonical": canonical(out)}
        # `pin_ids` opts a case into checking the actual agent NAMES, not just the
        # id-independent shape (README.md §6). Off by default so the id-blind cases
        # stay id-blind — an implementation free to name agents its own way still
        # passes those, and only opts in where reproducible identity is the point.
        if case.get("pin_ids"):
            result["ids"] = sorted(out.agents)
        # confluence check (reference-side law): N randomized schedules, same form —
        # and, when ids are pinned, the same ids too, which is the stronger property
        for seed in range(case.get("schedules", 0)):
            rng = random.Random(seed)
            alt = reducer.reduce(net, max_steps=case.get("max_steps", 100_000),
                                 opaque=set(case.get("opaque") or ()),
                                 strict_locality=strict,
                                 pick=rng.choice)
            if canonical(alt) != result["canonical"]:
                return {"error": f"NOT CONFLUENT (schedule seed {seed} diverged)"}
            if case.get("pin_ids") and sorted(alt.agents) != result["ids"]:
                return {"error": f"SCHEDULE-DEPENDENT IDS (schedule seed {seed} diverged)"}
        return result
    except BoxError as e:                      # a BoxError IS a NetError: check it first
        # The kind is a FIELD. It used to be sniffed out of the message, so rewording a
        # diagnostic silently reclassified a case — the one thing a conformance runner
        # must not do (Void Unity, 2026-08-29).
        return {"error": e.kind}
    except NetError:
        return {"error": "adapter-ports"}
    except ReduceError as e:
        msg = str(e)
        if "max_steps" in msg:
            return {"error": "termination-guard"}
        if "locality" in msg or "internal redex" in msg:
            return {"error": "locality"}
        return {"error": f"reduce-error: {msg}"}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--regen", action="store_true",
                    help="write the reference outcome back into each case's `expect`")
    ap.add_argument("cases", nargs="*", help="specific case files (default: all)")
    args = ap.parse_args()

    files = args.cases or sorted(glob.glob(os.path.join(HERE, "cases", "*.json")))
    if not files:
        print("no cases found")
        sys.exit(1)

    failed = 0
    for f in files:
        with open(f, encoding="utf-8") as fh:
            case = json.load(fh)
        got = execute(case)
        if args.regen:
            case["expect"] = got
            with open(f, "w", encoding="utf-8", newline="\n") as fh:
                json.dump(case, fh, indent=2, ensure_ascii=False)
                fh.write("\n")
            print(f"[gen ] {os.path.basename(f)}  -> {list(got)[0]}")
            continue
        ok = got == case.get("expect")
        print(f"[{'ok ' if ok else 'FAIL'}] {os.path.basename(f)}")
        if not ok:
            failed += 1
            print(f"        expected: {_key(case.get('expect'))[:200]}")
            print(f"        got:      {_key(got)[:200]}")

    if not args.regen:
        print(f"\nREDUCE CONFORMANCE: {len(files) - failed}/{len(files)} cases pass")
        sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
