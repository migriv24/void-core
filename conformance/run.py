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
    # Read the case as BYTES and decode without newline translation. Python's
    # text mode rewrites \r\n to \n before the library ever sees it, which meant
    # this suite could not observe how the core treats a CR by construction — and
    # a suite that normalizes its own inputs cannot test what a host is handed.
    # Void Unity found the gap from the other side (2026-08-27): its C# runner
    # read 14-journal.vs, which was committed with CRLF, exactly as stored and
    # watched it fail while this runner called it green. Cases now arrive here
    # byte-for-byte, so 15-crlf.vs — deliberately stored with CRLF, see
    # .gitattributes — actually pins the answer §8 gives.
    src = open(path, "rb").read().decode("utf-8")
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
