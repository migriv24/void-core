"""
run.py — run the language-neutral Temper normalization conformance cases (see README.md).

Each case in cases/*.json is a pure data fixture: a temper spec (the same
`config.transform.temper` form the state document carries), input runes, and the expected
output runes — or an abstract error kind. This runner validates the Python reference
implementation (`temper/`) against the cases; a host implementing Temper in another
language ports THIS file (~90 lines) and runs the same JSON.

    python conformance/temper/run.py            # run all cases
    python conformance/temper/run.py --regen    # regenerate `expect` from the reference

Two **laws** are checked on every successful case, not just the ones that think to ask —
idempotence (`temper(temper(x)) == temper(x)`) and purity (the input is not mutated).
They are the whole reason Temper is a separate layer, so a port that passes the cases but
breaks a law is not conforming, and this runner will say so.

`--regen` is for authoring new cases: it writes what the reference produces back into the
fixture. Regenerated expectations must be eyeballed before committing — the reference is
the oracle, but a golden file is only as good as its review.
"""
from __future__ import annotations

import argparse
import copy
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, ROOT)  # the repo-root `voidcore` package wires the layer paths

from voidcore import temper_from_spec  # noqa: E402


def _key(x) -> str:
    """The one comparison/sort key: canonical JSON text (sorted keys, no whitespace)."""
    return json.dumps(x, sort_keys=True, separators=(",", ":"))


def execute(case: dict) -> dict:
    """Run one case against the reference; return {"runes": [...]} or {"error": kind}.
    Error kinds (README.md §4): unknown-rule | missing-arg | law-idempotence | law-purity."""
    try:
        pass_ = temper_from_spec(case["spec"])
    except ValueError as e:
        msg = str(e)
        return {"error": "unknown-rule" if "unknown rule" in msg else "missing-arg"}

    runes = case["input"]["runes"]
    before = copy.deepcopy(runes)
    out = pass_.runes(runes)

    # LAW — purity: the source runes are untouched (Temper returns new objects)
    if _key(runes) != _key(before):
        return {"error": "law-purity"}
    # LAW — idempotence: a second pass is a no-op
    if _key(pass_.runes(out)) != _key(out):
        return {"error": "law-idempotence"}
    return {"runes": out}


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
        ok = _key(got) == _key(case.get("expect"))
        print(f"[{'ok ' if ok else 'FAIL'}] {os.path.basename(f)}")
        if not ok:
            failed += 1
            print(f"        expected: {_key(case.get('expect'))[:200]}")
            print(f"        got:      {_key(got)[:200]}")

    if not args.regen:
        print(f"\nTEMPER CONFORMANCE: {len(files) - failed}/{len(files)} cases pass")
        sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
