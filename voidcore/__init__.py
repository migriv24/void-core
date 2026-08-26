"""
voidcore — the installable Python surface for Void Core.

Install **editable** from the repo root so updates transfer with no copying:

    pip install -e C:/Users/migri/Documents/Projects/VoidCore

Editable means this package points at the *live* source tree, so any change to the
C core (a rebuilt `libvoidcore.dll`), the binding, or a holiday is picked up on the
next run. Apps then do `from voidcore import VoidCore, LocalJsonHoliday` — no
hardcoded paths.

This is a thin re-export layer: the binding still lives at `bindings/python/` and the
holidays under `holidays/`, loaded here from the repo root (`ROOT`). Heavier holidays
(MeshDB needs `neo4j`) are exposed lazily via `holiday(name)` so importing this package
never fails on an optional dependency.
"""
from __future__ import annotations

import importlib.util
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # the repo root


def _load(modname: str, *relpath: str):
    """Load a module from the live repo by file path, under a private name (avoids
    clashing this package's name with `bindings/python/voidcore.py`)."""
    path = os.path.join(ROOT, *relpath)
    spec = importlib.util.spec_from_file_location(modname, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[modname] = mod
    spec.loader.exec_module(mod)
    return mod


# Make holiday packages + the scry layer importable by their own module names (their
# intra-imports expect to be on the path), without disturbing apps that add these dirs.
for _p in [os.path.join(ROOT, "holidays", _h) for _h in ("localjson", "graph", "okf", "meshdb")] + \
          [os.path.join(ROOT, "scry"), os.path.join(ROOT, "temper"),
           os.path.join(ROOT, "reduce")]:
    if os.path.isdir(_p) and _p not in sys.path:
        sys.path.append(_p)

# ── eager exports (no third-party deps) ──────────────────────────────────────────
_binding = _load("_voidcore_binding", "bindings", "python", "voidcore.py")
VoidCore = _binding.VoidCore
# pure string helper (SPEC 6.1) - no engine, no DLL; importable on a bare checkout
quote_arg = _binding.quote_arg
split_args = _binding.split_args
split_transcript = _binding.split_transcript
UnterminatedQuote = _binding.UnterminatedQuote
CONTROL_WORDS = _binding.CONTROL_WORDS
from localjson_holiday import LocalJsonHoliday, RecordSchema  # noqa: E402
from roundtrip import check_roundtrip, RoundTripReport  # noqa: E402  (scry round-trip law)
from lens import Lens, pipeline  # noqa: E402  (scry: bidirectional projection + law)
from projection import (  # noqa: E402  (scry projection layer)
    scry, materialize, provenance, tag_match, dedupe_by, Context, Selector,
)
from temper import (  # noqa: E402  (temper normalization layer)
    Temper, dedupe, member_or_default, default_content, default_tag,
    normalize_tags, single_tag,
)
from net import Agent, Net, NetError, Port, to_net, from_net  # noqa: E402  (reduce: net)
from reduce import (  # noqa: E402  (reduce: interaction-net executor)
    Reducer, Rewrite, ReduceError, A, B, annihilate, commute, expand,
)
from .dispatch import Dispatcher  # noqa: E402  (transformation-verb seam, SPEC §7)
from .spec import (  # noqa: E402  (data-authored transformation specs)
    temper_from_spec, selector_from_spec, reducer_from_spec,
    temper_rule_names, reduce_rule_names,
)


def holiday(name: str):
    """Lazily fetch a holiday class by name (so optional deps load only on demand).

        holiday("localjson") -> LocalJsonHoliday
        holiday("meshdb")    -> MeshDBHoliday      (needs `neo4j`)
        holiday("graph")     -> GraphAnalyticsHoliday
    """
    if name == "localjson":
        return LocalJsonHoliday
    if name == "meshdb":
        from meshdb_holiday import MeshDBHoliday
        return MeshDBHoliday
    if name == "graph":
        from holiday import GraphAnalyticsHoliday  # holidays/graph/holiday.py
        return GraphAnalyticsHoliday
    raise ValueError(f"unknown holiday: {name}")


__all__ = ["VoidCore", "quote_arg", "split_args", "split_transcript",
           "UnterminatedQuote", "CONTROL_WORDS", "LocalJsonHoliday", "RecordSchema", "holiday",
           "check_roundtrip", "RoundTripReport", "Lens", "pipeline",
           "scry", "materialize", "provenance", "tag_match", "dedupe_by",
           "Context", "Selector",
           "Temper", "dedupe", "member_or_default", "default_content",
           "default_tag", "normalize_tags", "single_tag",
           "Reducer", "Rewrite", "ReduceError", "A", "B",
           "annihilate", "commute", "expand",
           "Agent", "Net", "NetError", "Port", "to_net", "from_net",
           "Dispatcher", "temper_from_spec", "selector_from_spec", "reducer_from_spec",
           "temper_rule_names", "reduce_rule_names", "ROOT"]
# Derived, not written: this is the FIFTH place a version lives, and it had drifted
# five releases behind the other four (0.1.0 vs 0.2.6) while being the one a host
# actually sees. Reading pyproject.toml is the same move as VC_VERSION_STR living
# once in vc_internal.h -- it cannot fork. `version_test.py` checks all five agree.
def _repo_version(default: str = "0.0.0") -> str:
    try:
        import re
        with open(os.path.join(ROOT, "pyproject.toml"), encoding="utf-8") as fh:
            m = re.search(r'^version\s*=\s*"([^"]+)"', fh.read(), re.M)
        return m.group(1) if m else default
    except OSError:
        return default          # installed without the source tree beside it


__version__ = _repo_version()
