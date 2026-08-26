"""
pure_import_test.py — the pure-Python surface imports with NO compiled engine.

    python voidcore/pure_import_test.py

Void Reyna (2026-08-17) duplicated ~50 lines of `scry/lens.py` rather than depend on
this package, because their phase-1 promise is `dependencies = []`: an evidence locker
has to work on a fresh checkout with nothing installed and no build. *"The thing that
proves what a document said in 2026 should not need a working toolchain in 2031."*

That constraint is already satisfied — `ctypes.CDLL` is called inside
`VoidCore.__init__`, not at module scope, so importing the package never touches
`libvoidcore`. This test makes it a **guarantee rather than an accident**: it poisons
`ctypes.CDLL` so any load-on-import fails loudly, then imports the package and
exercises the pure surface. Move a `CDLL(...)` to module scope and this test says so.

The guaranteed-pure surface: `Lens`, `pipeline`, `check_roundtrip`, `RoundTripReport`,
`quote_arg`, and the Scry/Temper/Reduce layers. `VoidCore` itself of course needs the
library — but only when you construct one.
"""
from __future__ import annotations

import ctypes
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def main() -> int:
    def _poisoned(*a, **k):
        raise AssertionError(
            "a shared library was loaded during import — the pure-Python surface is no "
            "longer importable without a build (look for a CDLL call at module scope)")

    real_cdll = ctypes.CDLL
    ctypes.CDLL = _poisoned
    try:
        import voidcore
    finally:
        ctypes.CDLL = real_cdll
    print("import voidcore with CDLL poisoned: OK (no library loaded)")

    # the surface a host may depend on without a toolchain
    for name in ("Lens", "pipeline", "check_roundtrip", "RoundTripReport", "quote_arg",
                 "scry", "materialize", "provenance", "tag_match", "Temper", "Reducer"):
        assert hasattr(voidcore, name), f"missing from the pure surface: {name}"

    # and it must actually WORK, not merely import
    lens = voidcore.Lens(forward=lambda r: {"v": r["a"]}, backward=lambda p: {"a": p["v"]})
    assert lens.check([{"a": 1}, {"a": 2}]).ok
    assert voidcore.pipeline().forward({"a": 1}) == {"a": 1}
    BS = chr(92)   # built, not written: a literal backslash here is at the mercy
                   # of every editor and shell between this file and the disk
    assert voidcore.quote_arg("don't") == "'don" + BS + "'t'"
    assert voidcore.tag_match({"spirit": {"name": "x"}, "tags": ["t"]}, "t")
    assert voidcore.Temper([voidcore.dedupe("xs")]).rune(
        {"content": {"xs": [1, 1, 2]}})["content"]["xs"] == [1, 2]
    print("pure surface usable without an engine: OK "
          "(Lens, pipeline, quote_arg, tag_match, Temper)")

    # `scry/lens.py` + `scry/roundtrip.py` are also stdlib-only on their own, so a host
    # that wants zero repo coupling can vendor exactly those two files.
    import importlib.util
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    for mod in ("roundtrip", "lens"):
        spec = importlib.util.spec_from_file_location(
            f"_standalone_{mod}", os.path.join(root, "scry", f"{mod}.py"))
        m = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = m
        spec.loader.exec_module(m)
    print("scry/roundtrip.py + scry/lens.py load standalone: OK (vendorable as two files)")

    print("\nPURE IMPORT: OK (no engine, no build, no third-party deps)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
