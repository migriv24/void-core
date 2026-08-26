"""
version_test.py — the version lives in five places; they must agree.

    python voidcore/version_test.py

Void Core's version is written in `pyproject.toml`, `package.json`, the C header
(`VC_VERSION_STR`), the app manifest (`okf/app.md`), and the Python package
(`voidcore.__version__`). Five copies of one fact drift, and this project has been bitten
twice: 0.2.4 found `vc_version()` still reporting "0.2.1" and `app.md` reporting "0.1.0",
and 2026-08-17 found `voidcore.__version__` five releases behind at "0.1.0" — the one a
host actually reads. Each time the fix was to stop writing it twice; this test is what
notices when a new copy appears.

It also checks the **built library** when one is available, because bumping
`VC_VERSION_STR` in source without rebuilding leaves the shipped DLL lying — the exact
0.2.4 failure. That check is skipped (not failed) when no library is present, so this
test still runs on a fresh checkout.
"""
from __future__ import annotations

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)


def _read(*rel: str) -> str:
    # utf-8-sig: package.json carries a BOM, which plain utf-8 hands to json.loads
    with open(os.path.join(ROOT, *rel), encoding="utf-8-sig") as fh:
        return fh.read()


def main() -> int:
    found: dict[str, str] = {}

    found["pyproject.toml"] = re.search(
        r'^version\s*=\s*"([^"]+)"', _read("pyproject.toml"), re.M).group(1)
    found["package.json"] = json.loads(_read("package.json"))["version"]
    found["VC_VERSION_STR"] = re.search(
        r'#define\s+VC_VERSION_STR\s+"([^"]+)"', _read("core", "src", "vc_internal.h")).group(1)
    found["okf/app.md"] = re.search(
        r'^version:\s*(\S+)', _read("okf", "app.md"), re.M).group(1)

    import voidcore
    found["voidcore.__version__"] = voidcore.__version__

    for where, v in found.items():
        print(f"  {where:<24} {v}")

    distinct = set(found.values())
    assert len(distinct) == 1, f"version drift across {len(distinct)} values: {found}"
    version = distinct.pop()

    # the built library, if there is one: source may say 0.2.6 while the DLL still says 0.2.5
    try:
        vc = voidcore.VoidCore()
    except (FileNotFoundError, OSError) as exc:
        print(f"  {'built library':<24} (skipped — {type(exc).__name__})")
    else:
        built = vc.version          # a @property, not a method
        vc.close()
        print(f"  {'built library':<24} {built}")
        assert built == version, (
            f"the built library reports {built!r} but the source says {version!r} — "
            f"rebuild it (cmake --build core/build)")

    print(f"\nVERSION: OK (all five agree on {version})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
