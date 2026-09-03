"""
describe_verbs_test.py — the `help` verb list must name every verb the core answers to.

Void Hormiga, 2026-09-02: `link` was missing from the flat `verbs` string that
`help`/`--describe` prints. The rest of that briefing is introspected from live
registries and is trustworthy; this one list is hand-written, so it drifts
silently, and an agent reading it concludes a verb does not exist. The same hole
had also swallowed `links`, `unlink` and `journal`.

There is nothing to introspect — the router is an if/else chain across the
verb-family files — so the check is made against the source instead: every
`!strcmp(v, "<verb>")` the families compare against must appear in the string,
and the string must not promise a verb no family answers to.

    python bindings/python/describe_verbs_test.py
"""
from __future__ import annotations

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from voidcore import VoidCore  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FAMILIES = os.path.join(ROOT, "core", "src", "dispatch")

# Handled outside the verb families: `exit` is an alias target the host acts on
# (the core never sees it), and the POSIX aliases desugar before dispatch.
NOT_A_CORE_VERB = {"exit"}

FAIL: list[str] = []


def check(cond: bool, msg: str) -> None:
    if not cond:
        FAIL.append(msg)


def dispatched_verbs() -> set[str]:
    """Every verb the family files compare the incoming verb against."""
    found: set[str] = set()
    for name in sorted(os.listdir(FAMILIES)):
        if not name.startswith("verbs_") or not name.endswith(".c"):
            continue
        src = open(os.path.join(FAMILIES, name), encoding="utf-8").read()
        found |= set(re.findall(r'!strcmp\(v,\s*"([a-z?-]+)"\)', src))
    return found


def main() -> int:
    vc = VoidCore()
    listed = set(vc.dispatch("help")["data"].split())
    real = dispatched_verbs()

    check(bool(real), "could not read any verb from the family sources")

    missing = sorted(real - listed - NOT_A_CORE_VERB)
    check(not missing,
          f"dispatched but absent from the `help` list: {missing} — "
          f"add them to `verbs` in core/src/dispatch/verbs_query.c")

    phantom = sorted(listed - real - NOT_A_CORE_VERB)
    check(not phantom,
          f"promised by the `help` list but no family answers to them: {phantom}")

    # The four Hormiga's report turned up, pinned by name so a regression names itself.
    for v in ("link", "links", "unlink", "journal"):
        check(v in listed, f"`{v}` missing from the help verb list (2026-09-02 regression)")
        check(vc.dispatch(v if v == "journal" else f"{v} --help") is not None,
              f"`{v}` did not dispatch at all")

    if FAIL:
        print("DESCRIBE VERBS: FAIL")
        for f in FAIL:
            print("  -", f)
        return 1
    print(f"DESCRIBE VERBS: OK ({len(real)} dispatched verbs, all listed; no phantoms)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
