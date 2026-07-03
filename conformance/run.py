"""
run.py — run the language-neutral conformance cases (SPEC §11).

Each case in cases/*.vs is a self-checking Voidscript script (SPEC §8 core subset):
it builds its own fixture, asserts the behaviors it targets (a false `assert` halts 1
at the first violation), and ends with `return <case>-ok`. A case passes iff
`script run` returns ok with data == "<case>-ok".

The default target is the C core through the Python binding; point --dll at any
other build of the library to conformance-test that instead. The script source is
delivered through the §2 state document (`scripts` map), so loading a state document
is itself under test in every case.
"""
from __future__ import annotations

import argparse
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "bindings", "python"))
from voidcore import VoidCore  # noqa: E402


def run_case(path: str, dll: str | None = None) -> tuple[bool, dict]:
    src = open(path, encoding="utf-8").read()
    name = os.path.splitext(os.path.basename(path))[0]
    vc = VoidCore(state={"version": 1, "scripts": {"case": src}}, dll_path=dll)
    try:
        res = vc.dispatch("script run case")
    finally:
        vc.close()
    return bool(res["ok"]) and res["data"] == f"{name}-ok", res


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--dll", default=None, help="path to an alternative libvoidcore build")
    ap.add_argument("cases", nargs="*", help="specific case files (default: all in cases/)")
    args = ap.parse_args()

    files = args.cases or sorted(glob.glob(os.path.join(HERE, "cases", "*.vs")))
    if not files:
        print("no cases found"); sys.exit(1)

    failed = 0
    for f in files:
        ok, res = run_case(f, args.dll)
        print(f"[{'ok ' if ok else 'FAIL'}] {os.path.basename(f)}")
        if not ok:
            failed += 1
            for line in res["lines"][-8:]:
                print(f"        {line}")
            print(f"        data = {res['data']!r}")

    print(f"\nCONFORMANCE: {len(files) - failed}/{len(files)} cases pass")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
