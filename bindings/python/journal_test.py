"""
journal_test.py — the command journal (SPEC §6.2): reified commands.

Covers the properties a consumer actually builds against:
  1. off by default, and enabling it changes nothing about what commands do;
  2. `minted` names the ids the command introduced, so replaying the ENTRY is
     deterministic where replaying the command TEXT is not;
  3. `pure` is false exactly when the command could cross the holiday boundary —
     statically, so two peers agree, and observed through `batch`;
  4. `command` is canonical, so one change never records under two spellings.

    python bindings/python/journal_test.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from voidcore import VoidCore

FAILS: list[str] = []


def check(label: str, cond: bool, detail: str = "") -> None:
    if cond:
        print(f"  ok   {label}")
    else:
        FAILS.append(label)
        print(f"  FAIL {label}{(': ' + detail) if detail else ''}")


def ids_in(state: dict) -> set[str]:
    out = set()
    for m in state.get("mantles", []):
        out.add(m.get("id", ""))
        for r in m.get("runes", []):
            out.add(r.get("spirit", {}).get("id", ""))
    for b in state.get("bindings", []):
        out.add(b.get("id", ""))
    return out - {""}


def test_off_by_default() -> None:
    print("\n[off by default]")
    vc = VoidCore()
    vc.dispatch("mantle new alpha")
    check("no entries when never enabled", vc.journal() == [])
    vc.close()


def test_records_and_mints() -> None:
    print("\n[records + minted ids]")
    vc = VoidCore()
    vc.set_journal(True)
    before = ids_in(vc.export_state())
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    r = vc.dispatch("rune new text title")
    check("the rune was created", r["ok"], str(r))
    after = ids_in(vc.export_state())

    j = vc.journal()
    verbs = [e["verb"] for e in j]
    check("mantle new + rune new recorded", verbs == ["mantle", "rune"], str(verbs))
    check("seq is 1-based and dense", [e["seq"] for e in j] == [1, 2], str(j))

    minted = {i for e in j for i in e["minted"]}
    check("minted == exactly the new ids", minted == (after - before),
          f"{sorted(minted)} vs {sorted(after - before)}")
    check("every entry is pure", all(e["pure"] for e in j), str(j))
    check("slice is the undo slice", all(e["slice"] == "undo" for e in j), str(j))
    check("who is null with no actor", all(e["who"] is None for e in j), str(j))
    vc.close()


def test_replay_is_deterministic() -> None:
    """The property that makes the entry worth more than the command string: the
    text alone mints a DIFFERENT id every time; the entry does not."""
    print("\n[the entry pins identity, the text does not]")
    a, b = VoidCore(), VoidCore()
    for vc in (a, b):
        vc.set_journal(True)
        vc.dispatch("mantle new alpha")
        vc.dispatch("use alpha")
        vc.dispatch("rune new text title")
    ja, jb = a.journal(), b.journal()
    check("same commands recorded",
          [e["command"] for e in ja] == [e["command"] for e in jb])
    check("but DIFFERENT ids minted (so the text is not enough)",
          [e["minted"] for e in ja] != [e["minted"] for e in jb],
          f"{ja[-1]['minted']} vs {jb[-1]['minted']}")
    check("and the entry carries the id that was actually used",
          ja[-1]["minted"][0] in ids_in(a.export_state()))
    a.close()
    b.close()


def test_pure_vs_effectful() -> None:
    print("\n[pure vs effectful — the holiday boundary]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.set_effect_handler(lambda op, args: {"wrote": True})
    vc.dispatch("mantle new alpha")
    vc.dispatch("save")
    j = vc.journal()
    by_verb = {e["verb"]: e for e in j}
    check("mantle new is pure", by_verb["mantle"]["pure"] is True)
    check("save is recorded", "save" in by_verb, str(j))
    check("save landed on no state slice",
          by_verb.get("save", {}).get("slice") == "host", str(j))
    check("save is NOT pure", by_verb.get("save", {}).get("pure") is False, str(j))
    vc.close()


def test_static_classification() -> None:
    """save must classify identically with and without a handler registered —
    otherwise the same command is a recordable change on one peer and not the
    other, which is the divergence the distinction exists to prevent."""
    print("\n[classification is static, not host-dependent]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    vc.dispatch("save")            # no effect handler registered at all
    j = vc.journal()
    save = [e for e in j if e["verb"] == "save"]
    check("save records even with no handler", len(save) == 1, str(j))
    check("save is effectful anyway", bool(save) and save[0]["pure"] is False,
          str(save))
    vc.close()


def test_batch_observes_effects() -> None:
    """A batch is one entry (one undo frame, one record), and it is pure only if
    nothing inside it reached the host."""
    print("\n[batch: one entry, purity observed through it]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.set_effect_handler(lambda op, args: None)
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    vc.journal_clear()

    r = vc.dispatch("""batch '["rune new text a", "rune new text b"]'""")
    j = vc.journal()
    check("pure batch -> one entry", r["ok"] and len(j) == 1, f"{r} / {j}")
    check("pure batch carries BOTH minted ids", len(j[0]["minted"]) == 2, str(j))
    check("pure batch is pure", j[0]["pure"] is True, str(j))

    vc.journal_clear()
    vc.dispatch("""batch '["rune new text c", "save"]'""")
    j2 = vc.journal()
    check("effectful batch -> one entry", len(j2) == 1, str(j2))
    check("a batch containing save is NOT pure",
          bool(j2) and j2[0]["pure"] is False, str(j2))
    vc.close()


def test_canonical_command() -> None:
    print("\n[the recorded line is canonical]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    vc.dispatch("rune new text victim")
    vc.journal_clear()
    vc.dispatch("rm victim")          # the POSIX alias for: rune rm victim
    j = vc.journal()
    check("alias recorded in canonical form",
          bool(j) and j[0]["command"] == "rune rm victim", str(j))
    check("verb is the canonical verb", bool(j) and j[0]["verb"] == "rune", str(j))
    vc.close()


def test_rewind_is_recorded() -> None:
    """A record that omits taking a change back replays into a state the author
    never had."""
    print("\n[undo/redo appear in the record]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    vc.dispatch("rune new text title")
    vc.dispatch("undo")
    vc.dispatch("redo")
    verbs = [e["verb"] for e in vc.journal()]
    check("undo and redo recorded",
          verbs == ["mantle", "rune", "undo", "redo"], str(verbs))
    vc.journal_clear()
    fail = vc.dispatch("redo")
    check("a failed redo records nothing",
          not fail["ok"] and vc.journal() == [], str(vc.journal()))
    vc.close()


def test_failed_command_records_nothing() -> None:
    print("\n[failure records nothing]")
    vc = VoidCore()
    vc.set_journal(True)
    r = vc.dispatch("rune new text orphan")   # no active mantle
    check("the command failed", not r["ok"], str(r))
    check("nothing recorded", vc.journal() == [], str(vc.journal()))
    vc.close()


def test_attribution() -> None:
    print("\n[who]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("config set actor ada")
    vc.dispatch("mantle new alpha")
    j = vc.journal()
    check("entry carries the actor", bool(j) and j[0]["who"] == "ada", str(j))
    vc.close()


def test_clear_keeps_seq() -> None:
    print("\n[clear does not reuse names]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    first = vc.journal()[0]["seq"]
    vc.journal_clear()
    check("cleared", vc.journal() == [])
    vc.dispatch("mantle new beta")
    check("seq keeps counting after a clear", vc.journal()[0]["seq"] > first,
          str(vc.journal()))
    vc.close()


def test_view_slice() -> None:
    print("\n[the view slice records as view]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    vc.dispatch("rune new text title")
    vc.journal_clear()
    r = vc.dispatch("place title 10 20")
    j = vc.journal()
    check("place succeeded", r["ok"], str(r))
    check("place recorded on the view slice",
          bool(j) and j[0]["slice"] == "view", str(j))
    check("place is pure", bool(j) and j[0]["pure"] is True, str(j))
    check("place mints nothing", bool(j) and j[0]["minted"] == [], str(j))
    vc.close()


def test_snapshot_is_not_an_identity() -> None:
    """`save` copies `mantles` into `_baseline`. If the id-set diff counts that
    snapshot, `save` reports every id in the document as freshly minted — an
    entry claiming a dozen creations that never happened."""
    print("\n[a snapshot is not an identity]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    vc.dispatch("rune new text a")
    vc.dispatch("rune new text b")
    vc.journal_clear()
    vc.dispatch("save")
    j = vc.journal()
    check("save mints nothing", bool(j) and j[0]["minted"] == [], str(j))

    # revert restores from the baseline, and what it puts back IS introduced
    vc.dispatch("rm a")
    vc.journal_clear()
    vc.dispatch("revert")
    j2 = vc.journal()
    check("revert reports the id it restored",
          bool(j2) and len(j2[0]["minted"]) == 1, str(j2))
    vc.close()


def test_content_is_opaque() -> None:
    """Content is opaque by contract, so an app field named `id` inside it must
    not be counted as an identity the core minted."""
    print("\n[content stays opaque to the id diff]")
    vc = VoidCore()
    vc.set_journal(True)
    vc.dispatch("mantle new alpha")
    vc.dispatch("use alpha")
    vc.dispatch("rune new text a")
    vc.journal_clear()
    r = vc.dispatch('setjson a payload \'{"id": "not-ours-42"}\'')
    j = vc.journal()
    check("the setjson succeeded", r["ok"], str(r))
    check("an app-defined content id is not counted",
          bool(j) and j[0]["minted"] == [], str(j))
    vc.close()


def test_enabling_changes_nothing() -> None:
    """Journaling is a record, not a behaviour change: the same script must
    produce the same state document either way (ids aside)."""
    print("\n[enabling the journal changes no behaviour]")
    script = ["mantle new alpha", "use alpha", "rune new text title",
              "set title text hello", "tag title draft", "undo"]
    outs = []
    for on in (False, True):
        vc = VoidCore()
        if on:
            vc.set_journal(True)
        results = [vc.dispatch(c) for c in script]
        st = vc.export_state()
        for m in st.get("mantles", []):
            m["id"] = "<id>"
            for r in m.get("runes", []):
                r["spirit"]["id"] = "<id>"
        outs.append(([r["ok"] for r in results], st))
        vc.close()
    check("same ok flags", outs[0][0] == outs[1][0], f"{outs[0][0]} vs {outs[1][0]}")
    check("same resulting state", outs[0][1] == outs[1][1])


def main() -> int:
    print("journal_test — SPEC §6.2 reified commands")
    for t in (test_off_by_default, test_records_and_mints,
              test_replay_is_deterministic, test_pure_vs_effectful,
              test_static_classification, test_batch_observes_effects,
              test_canonical_command, test_rewind_is_recorded,
              test_failed_command_records_nothing, test_attribution,
              test_clear_keeps_seq, test_view_slice,
              test_snapshot_is_not_an_identity, test_content_is_opaque,
              test_enabling_changes_nothing):
        t()
    print()
    if FAILS:
        print(f"FAILED ({len(FAILS)}): " + ", ".join(FAILS))
        return 1
    print("all journal checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
