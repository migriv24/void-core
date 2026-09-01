"""
undo_control_test.py — host-controlled undo (SPEC §6): vc_set_undo / vc_set_undo_depth.

Undo is memento-based: every mutating command copies the whole undoable slice
before it runs. That is cheap for a document and expensive for a world, and only
the host knows which one its `mantles` hold. Void Unity measured the difference
on 2026-08-28 — a `set` at 4 000 runes cost 27.6 ms, longer than a 60 Hz frame,
and building that world was quadratic — and named the asymmetry: the journal has
had an off switch since the day it shipped, undo had none.

Covers the properties a host builds against:
  1. on by default, so no existing host changes behavior;
  2. off means no frame is taken — measurable, not merely reported;
  3. `undo`/`redo` fail with a reason, not with "nothing to undo";
  4. `batch` stays atomic with undo off (it rolls back from its own copy, not
     from the undo stack) — the property that would be silently lost if the two
     mechanisms were actually one;
  5. depth bounds the stack and lowering it trims immediately;
  6. turning undo back on resumes recording from that point.

    python bindings/python/undo_control_test.py
"""
from __future__ import annotations

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from voidcore import VoidCore

FAILS: list[str] = []


def check(label: str, cond: bool, detail: str = "") -> None:
    if cond:
        print(f"  ok   {label}")
    else:
        FAILS.append(label)
        print(f"  FAIL {label}" + (f"  — {detail}" if detail else ""))


def fixture(undo=None, depth=None) -> VoidCore:
    vc = VoidCore()
    if undo is not None:
        vc.set_undo(undo)
    if depth is not None:
        vc.set_undo_depth(depth)
    vc.dispatch("mantle new m")
    vc.dispatch("rune new text r")
    return vc


def test_on_by_default() -> None:
    print("on by default")
    vc = fixture()
    vc.dispatch("set r v first")
    vc.dispatch("set r v second")
    check("undo walks back", vc.dispatch("undo")["ok"])
    check("value restored", vc.dispatch("get r v --json")["data"] == "first")
    check("history is not empty", len(vc.dispatch("history")["data"]) > 0)
    vc.close()


def test_off_records_nothing() -> None:
    print("off records nothing")
    vc = fixture(undo=False)
    vc.dispatch("set r v first")
    vc.dispatch("set r v second")
    check("history empty", vc.dispatch("history")["data"] == [])
    r = vc.dispatch("undo")
    check("undo fails", not r["ok"])
    check("and says why", "disabled" in r["lines"][0], r["lines"][0])
    check("redo says why", "disabled" in vc.dispatch("redo")["lines"][0])
    check("value untouched", vc.dispatch("get r v --json")["data"] == "second")
    vc.close()


def test_off_drops_existing_frames() -> None:
    """Unlike the journal, which keeps its entries: an undo frame is only ever
    consumed by `undo`, so keeping them would hold exactly the memory declined."""
    print("off drops existing frames")
    vc = fixture()
    base = len(vc.dispatch("history")["data"])  # the fixture's own mutations
    vc.dispatch("set r v first")
    check("frame recorded", len(vc.dispatch("history")["data"]) == base + 1)
    vc.set_undo(False)
    check("frames dropped", vc.dispatch("history")["data"] == [])
    vc.set_undo(True)
    check("nothing resurrected", vc.dispatch("history")["data"] == [])
    vc.dispatch("set r v second")
    check("recording resumes", len(vc.dispatch("history")["data"]) == 1)
    check("undo works again", vc.dispatch("undo")["ok"])
    check("back to first", vc.dispatch("get r v --json")["data"] == "first")
    vc.close()


def test_batch_stays_atomic() -> None:
    """The one property that would break if batch rollback rode on the undo
    stack. It does not — verbs_script.c saves its own copy — and this pins it."""
    print("batch stays atomic with undo off")
    for undo in (True, False):
        vc = fixture(undo=undo)
        vc.dispatch("set r v before")
        bad = '["set r v after","rune new text r","set r v never"]'
        r = vc.dispatch(f"batch '{bad}'")
        check(f"batch fails (undo={undo})", not r["ok"])
        check(f"rolled back (undo={undo})",
              vc.dispatch("get r v --json")["data"] == "before",
              str(vc.dispatch("get r v --json")["data"]))
        vc.close()


def test_depth_bounds_the_stack() -> None:
    print("depth bounds the stack")
    vc = fixture(depth=3)
    for i in range(10):
        vc.dispatch(f"set r v {i}")
    check("kept 3 frames", len(vc.dispatch("history")["data"]) == 3,
          str(len(vc.dispatch("history")["data"])))
    vc.dispatch("undo 3")
    check("undid exactly 3", vc.dispatch("get r v --json")["data"] == "6")
    check("nothing older survives", not vc.dispatch("undo")["ok"])
    vc.close()


def test_lowering_depth_trims_now() -> None:
    """A depth change that waited for the next mutation to take effect would not
    be a depth change — the frames are the cost, and they are resident now."""
    print("lowering depth trims immediately")
    vc = fixture()
    base = len(vc.dispatch("history")["data"])
    for i in range(10):
        vc.dispatch(f"set r v {i}")
    check("10 more frames", len(vc.dispatch("history")["data"]) == base + 10)
    vc.set_undo_depth(2)
    check("trimmed to 2 without a mutation",
          len(vc.dispatch("history")["data"]) == 2,
          str(len(vc.dispatch("history")["data"])))
    vc.set_undo_depth(0)
    check("depth 0 clamps to 1", len(vc.dispatch("history")["data"]) == 1)
    vc.close()


def test_off_is_actually_cheaper() -> None:
    """The finding was a measurement, so the fix gets one. The memento copies the
    whole undoable slice, so with undo ON a `set` costs O(runes) and building n
    runes costs O(n²); with it OFF both are flat. The threshold is deliberately
    loose — this asserts a change of ORDER, not a timing budget."""
    print("off is actually cheaper (the finding was a measurement)")
    n = 600

    def build_ms(undo: bool) -> float:
        vc = VoidCore()
        vc.set_undo(undo)
        vc.dispatch("mantle new w")
        t0 = time.perf_counter()
        for i in range(n):
            vc.dispatch(f"rune new text r{i}")
        ms = (time.perf_counter() - t0) * 1000
        vc.close()
        return ms

    def set_us(undo: bool) -> float:
        vc = VoidCore()
        vc.set_undo(undo)
        vc.dispatch("mantle new w")
        for i in range(n):
            vc.dispatch(f"rune new text r{i}")
        t0 = time.perf_counter()
        for i in range(100):
            vc.dispatch(f"set r0 v {i}")
        us = (time.perf_counter() - t0) * 1e6 / 100
        vc.close()
        return us

    b_on, b_off = build_ms(True), build_ms(False)
    s_on, s_off = set_us(True), set_us(False)
    print(f"       build {n}: {b_on:.0f} ms on / {b_off:.0f} ms off")
    print(f"       set at {n}: {s_on:.0f} us on / {s_off:.0f} us off")
    check("build is markedly cheaper with undo off", b_off * 4 < b_on,
          f"{b_on:.0f} vs {b_off:.0f} ms")
    check("set is markedly cheaper with undo off", s_off * 4 < s_on,
          f"{s_on:.0f} vs {s_off:.0f} us")


def main() -> int:
    print(f"libvoidcore {VoidCore().version}\n")
    for t in (test_on_by_default, test_off_records_nothing,
              test_off_drops_existing_frames, test_batch_stays_atomic,
              test_depth_bounds_the_stack, test_lowering_depth_trims_now,
              test_off_is_actually_cheaper):
        t()
    print()
    if FAILS:
        print(f"FAILED ({len(FAILS)}): " + ", ".join(FAILS))
        return 1
    print("all undo-control checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
