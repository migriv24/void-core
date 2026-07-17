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

from voidcore import Net, NetError, ReduceError, reducer_from_spec, to_net  # noqa: E402


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


def execute(case: dict) -> dict:
    """Run one case against the reference; return {"canonical": ...} or {"error": kind}.
    Error kinds (README.md §5): adapter-ports | locality | termination-guard."""
    try:
        reducer, sigs = reducer_from_spec(case["spec"])
        net = to_net(case["input"], sigs)
        strict = bool(case.get("strict_locality", False))
        out = reducer.reduce(net,
                             max_steps=case.get("max_steps", 100_000),
                             opaque=set(case.get("opaque") or ()),
                             strict_locality=strict)
        result = {"canonical": canonical(out)}
        # confluence check (reference-side law): N randomized schedules, same form
        for seed in range(case.get("schedules", 0)):
            rng = random.Random(seed)
            alt = reducer.reduce(net, max_steps=case.get("max_steps", 100_000),
                                 opaque=set(case.get("opaque") or ()),
                                 strict_locality=strict,
                                 pick=rng.choice)
            if canonical(alt) != result["canonical"]:
                return {"error": f"NOT CONFLUENT (schedule seed {seed} diverged)"}
        return result
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
