"""
attribution_test.py — SPEC §9 attribution + the mutation spine, through the binding.

Covers: config.actor stamping `who` on log records, the who-suffix on history lines
(data stays a plain label array), the mutation-spine command echo, batch logging once,
undo/redo logging, and that no actor => no `who` (backward compatible).

    python bindings/python/attribution_test.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from voidcore import VoidCore


def main() -> int:
    vc = VoidCore()

    # no actor configured: mutations log on the spine, records carry no `who`
    assert vc.dispatch("mantle new studio")["ok"]
    recs = vc.dispatch("log")["data"]
    spine = [r for r in recs if r["op"] == "mantle"]
    assert spine and spine[-1]["msg"] == "mantle new studio"
    assert "who" not in spine[-1], "no actor => no who field"

    # session actor: every log record and undo frame is attributed
    assert vc.dispatch("config set actor agent:claude")["ok"]
    assert vc.dispatch("rune new text riff")["ok"]
    recs = vc.dispatch("log")["data"]
    last = [r for r in recs if r["op"] == "rune"][-1]
    assert last["who"] == "agent:claude" and last["msg"] == "rune new text riff"

    hist = vc.dispatch("history")
    assert hist["data"][-1] == "rune new text riff", "history data stays plain labels"
    assert hist["lines"][-1].endswith("[agent:claude]"), hist["lines"][-1]
    assert not any("[" in ln for ln in hist["lines"][:-1]), "pre-actor frames unattributed"

    # log lines render the actor; a second actor takes over mid-session
    assert vc.dispatch("config set actor human:kris")["ok"]
    assert vc.dispatch("set riff value C4")["ok"]
    lines = vc.dispatch("log")["lines"]
    assert any("set (human:kris): set riff value C4" in ln for ln in lines), lines[-3:]

    # batch logs once (sub-commands are inside the one frame)
    assert vc.dispatch("batch '[\"set riff value D4\", \"tag riff +bar:1\"]'")["ok"]
    recs = vc.dispatch("log")["data"]
    assert sum(1 for r in recs if r["op"] == "batch") == 1
    assert not any(r["msg"] == "set riff value D4" for r in recs), "sub-commands don't double-log"

    # undo/redo are logged and attributed
    assert vc.dispatch("undo")["ok"]
    assert vc.dispatch("redo")["ok"]
    recs = vc.dispatch("log")["data"]
    assert [r["who"] for r in recs if r["op"] in ("undo", "redo")] == ["human:kris"] * 2

    # a failed command logs nothing on the spine
    n = len(vc.dispatch("log")["data"])
    assert not vc.dispatch("set nosuchrune x y")["ok"]
    assert len(vc.dispatch("log")["data"]) == n

    # clearing the actor stops attribution
    assert vc.dispatch('config set actor ""')["ok"]
    assert vc.dispatch("set riff value E4")["ok"]
    last = vc.dispatch("log")["data"][-1]
    assert last["msg"] == "set riff value E4" and "who" not in last

    vc.close()
    print("ATTRIBUTION: OK (who on log records, history suffix, mutation spine, "
          "batch-once, undo/redo, actor lifecycle)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
