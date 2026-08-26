"""
run.py — run the language-neutral Scry projection conformance cases (see README.md).

Each case in cases/*.json is a pure data fixture naming an `op` (`scry`, `materialize`,
`provenance`, or `tag_match`), its inputs, and the expected result — or an abstract error
kind. This runner validates the Python reference implementation (`scry/`) against the
cases; a host implementing Scry in another language ports THIS file (~110 lines) and runs
the same JSON.

    python conformance/scry/run.py            # run all cases
    python conformance/scry/run.py --regen    # regenerate `expect` from the reference

Purity is checked on every successful case, not only where a case asks: Scry is the read
side and MUST NOT mutate the runes it projects.

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

from voidcore import (  # noqa: E402
    Context, materialize, provenance, selector_from_spec, tag_match,
)


def rune_name(rune: dict) -> str:
    return str(rune.get("spirit", {}).get("name", ""))


def _key(x) -> str:
    """The one comparison key: canonical JSON text (sorted keys, no whitespace)."""
    return json.dumps(x, sort_keys=True, separators=(",", ":"))


def _context(spec: dict | None) -> Context:
    spec = spec or {}
    return Context(locale=spec.get("locale"), audience=spec.get("audience"),
                   date=spec.get("date"), role=spec.get("role"),
                   extra=spec.get("extra") or {})


def execute(case: dict) -> dict:
    """Run one case against the reference. Returns the op's result object, or
    {"error": kind}. Error kinds (README.md §5): bad-selector | bad-into | unknown-op."""
    op = case.get("op", "scry")
    runes = case.get("input", {}).get("runes", [])
    before = copy.deepcopy(runes)

    try:
        result = _dispatch(op, case, runes)
    except ValueError as e:
        msg = str(e).lower()
        if "into must be" in msg:
            return {"error": "bad-into"}
        if "selector spec" in msg:
            return {"error": "bad-selector"}
        return {"error": f"error: {e}"}

    # LAW — purity: the read side never mutates what it read
    if _key(runes) != _key(before):
        return {"error": "law-purity"}
    return result


def _dispatch(op: str, case: dict, runes: list) -> dict:
    if op == "scry":
        # The data-expressible subset (where/sort/reverse/limit) never alters a rune —
        # it filters, orders and caps. So the observable is the resulting NAME SEQUENCE.
        sel = selector_from_spec(case.get("selector") or {})
        out = sel.run(runes, _context(case.get("context")))
        return {"names": [rune_name(r) for r in out]}

    if op == "tag_match":
        return {"names": [rune_name(r) for r in runes
                          if tag_match(r, case.get("expr"))]}

    if op == "materialize":
        out = materialize(runes, case.get("resolved") or {},
                          into=case.get("into", "content"),
                          stamp=case.get("stamp"))
        return {"runes": out}

    if op == "provenance":
        # A real byte-level commitment: canonical JSON (sorted keys, no whitespace,
        # non-ASCII kept as UTF-8), SHA-256, first 16 hex chars.
        return {"ids": [provenance(v) for v in case.get("values", [])]}

    return {"error": "unknown-op"}


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
        print(f"\nSCRY CONFORMANCE: {len(files) - failed}/{len(files)} cases pass")
        sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
