"""
ref_index_test.py — the ref index and the batch pre-image (0.2.15).

Resolving a ref (`set r0 ...`, `link a b`) used to walk the mantle's rune list,
so every command at N runes paid O(N), and an import of three commands per rune
was superquadratic: Void Hormiga measured 20 000 runes at 128 s (2026-10-25).
The core now keeps a name-or-id -> rune index for the active mantle.

An index is an ANSWER CACHE, and the only thing worth testing about a cache is
that it never answers differently from the thing it caches. Its keys are
borrowed from the runes' own strings, so a stale entry is not a wrong answer but
a dangling pointer. So this test is differential: after every command, every
ref is resolved through the index (`cat`, an index-safe verb) and compared with
the answer computed here from the exported document by the SPEC's rule — the
first rune in document order whose name or id equals the ref.

Covers:
  1. a seeded random walk over every verb that removes, renames, duplicates or
     replaces runes (rm, rename, dup, undo, redo, revert, mantle new/use/rm,
     failed and successful batches), with lookups between every step;
  2. the same inside ONE batch, where the index stays live across sub-commands;
  3. a name that equals another rune's id — first match in document order;
  4. a failed batch rolls back `glyphs` too (it used to keep a declaration);
  5. a failed batch with undo on rolls back from the memento, and the undo
     history is exactly as if the batch had never run;
  6. validate still reports every duplicate name, now in linear time.

    python bindings/python/ref_index_test.py
"""
from __future__ import annotations

import json
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from voidcore import VoidCore, quote_arg

FAILS: list[str] = []


def check(label: str, cond: bool, detail: str = "") -> None:
    if cond:
        print(f"  ok   {label}")
    else:
        FAILS.append(label)
        print(f"  FAIL {label}" + (f"  — {detail}" if detail else ""))


def active_runes(vc: VoidCore) -> list[dict] | None:
    st = vc.export_state()
    name = (st.get("active") or {}).get("mantle")
    for m in st.get("mantles", []):
        if m.get("name") == name:
            return m.get("runes", [])
    return None


def expected(runes: list[dict], ref: str) -> str | None:
    """SPEC §3.1: the first rune, in document order, answering to ref."""
    for r in runes:
        sp = r.get("spirit", {})
        if sp.get("name") == ref or sp.get("id") == ref:
            return sp.get("id")
    return None


def resolved(vc: VoidCore, ref: str) -> str | None:
    r = vc.dispatch("cat " + quote_arg(ref))
    if not r["ok"]:
        return None
    return json.loads("\n".join(r["lines"]))["spirit"]["id"]


def agree(vc: VoidCore, refs: list[str]) -> str:
    """'' when the index agrees with the document on every ref, else the first
    disagreement."""
    runes = active_runes(vc)
    if runes is None:
        return ""
    for ref in refs:
        want, got = expected(runes, ref), resolved(vc, ref)
        if want != got:
            return f"ref {ref!r}: document says {want}, index says {got}"
    return ""


def refs_of(vc: VoidCore, extra: list[str]) -> list[str]:
    runes = active_runes(vc) or []
    out = [r["spirit"]["name"] for r in runes] + [r["spirit"]["id"] for r in runes]
    return out + extra


NAMES = [f"n{i}" for i in range(12)]


def random_command(rng: random.Random, vc: VoidCore) -> str:
    n = lambda: rng.choice(NAMES)
    roll = rng.random()
    if roll < 0.22:
        return f"rune new text {n()}"
    if roll < 0.34:
        return f"set {n()} value {rng.randint(0, 99)}"
    if roll < 0.44:
        return f"rune rm {n()}"
    if roll < 0.54:
        return f"rune rename {n()} {n()}"
    if roll < 0.60:
        return f"rune dup {n()} {n()}"
    if roll < 0.66:
        return f"undo {rng.randint(1, 3)}"
    if roll < 0.70:
        return "redo"
    if roll < 0.73:
        return "save"
    if roll < 0.76:
        return "revert"
    if roll < 0.80:
        return f"use {rng.choice(['a', 'b'])}"
    if roll < 0.84:
        return f"link {n()} {n()}"
    if roll < 0.92:
        # A batch whose sub-commands keep the index live and then change the
        # rune set underneath it — sometimes ending in a failure, so rollback
        # replaces `mantles` while an index into the old one may exist.
        subs = [f"rune new text {n()}", f"set {n()} value x",
                f"rune rm {n()}", f"rune new text {n()}", f"cat {n()}"]
        rng.shuffle(subs)
        if rng.random() < 0.5:
            subs.append("set definitely-not-a-rune value 1")
        return "batch " + quote_arg(json.dumps(subs))
    if roll < 0.96:
        return f"rune rm @value"
    return f"mantle rm {rng.choice(['b'])}"


def test_random_walk() -> None:
    worst = ""
    for seed in range(40):
        rng = random.Random(seed)
        vc = VoidCore()
        vc.dispatch("mantle new b")
        vc.dispatch("mantle new a")
        for _ in range(120):
            cmd = random_command(rng, vc)
            vc.dispatch(cmd)
            if cmd.startswith("mantle rm"):
                vc.dispatch("mantle new b")
                vc.dispatch("use a")
            bad = agree(vc, refs_of(vc, NAMES + ["", "nope"]))
            if bad:
                worst = f"seed {seed} after {cmd!r}: {bad}"
                break
        vc.close()
        if worst:
            break
    check("index agrees with the document across 40 x 120 random commands",
          not worst, worst)


def test_inside_one_batch() -> None:
    vc = VoidCore()
    vc.dispatch("mantle new m")
    subs = []
    for i in range(50):
        subs += [f"rune new text r{i}", f"set r{i} value {i}"]
    subs += ["rune rm r10", "rune new text r10", "rune rename r11 z",
             "rune new text r11", "rune dup r12 q", "set r10 value again",
             "set z value renamed", "set q value copied", "set r11 value fresh"]
    r = vc.dispatch("batch " + quote_arg(json.dumps(subs)))
    check("a batch that removes and re-creates through a live index applies",
          r["ok"], str(r["lines"][:2]))
    runes = {x["spirit"]["name"]: x for x in active_runes(vc)}
    check("each write landed on the rune the name names AT THAT POINT",
          runes["r10"]["content"].get("value") == "again"
          and runes["z"]["content"].get("value") == "renamed"
          and runes["q"]["content"].get("value") == "copied"
          and runes["r11"]["content"].get("value") == "fresh",
          json.dumps({k: runes[k]["content"] for k in ("r10", "z", "q", "r11")}))
    check("and every ref still resolves as the document says",
          not agree(vc, refs_of(vc, ["r10", "r11", "z", "q", "gone"])))


def test_name_equal_to_another_id() -> None:
    vc = VoidCore()
    vc.dispatch("mantle new m")
    vc.dispatch("rune new text first")
    first_id = active_runes(vc)[0]["spirit"]["id"]
    r = vc.dispatch("rune new text " + first_id)
    check("a new rune may not take a name another rune answers to by id",
          not r["ok"], str(r["lines"]))
    vc.dispatch("rune new text second")
    vc.dispatch(f"rune rename second {first_id}")  # rename's own rule decides
    check("first match in document order wins either way",
          not agree(vc, refs_of(vc, [first_id])))


def test_failed_batch_rolls_back_glyphs() -> None:
    for undo in (True, False):
        vc = VoidCore()
        vc.set_undo(undo)
        vc.dispatch("mantle new m")
        decl = json.dumps({"glyph": "thing", "fields": ["a"]})
        r = vc.dispatch("batch " + quote_arg(json.dumps(
            ["glyph declare " + quote_arg(decl), "set nosuch a 1"])))
        st = vc.export_state()
        check(f"failed batch (undo {'on' if undo else 'off'}) leaves no declaration",
              not r["ok"] and "thing" not in st.get("glyphs", {}),
              str(list(st.get("glyphs", {}))))
        check(f"... and no rune can be made of the rolled-back glyph (undo "
              f"{'on' if undo else 'off'})",
              not vc.dispatch("rune new thing t")["ok"])


def test_failed_batch_with_memento() -> None:
    vc = VoidCore()
    vc.dispatch("mantle new m")
    vc.dispatch("rune new text keep")
    vc.dispatch("set keep value 1")
    before_state = vc.export_state()
    before_hist = vc.dispatch("history")["data"]
    r = vc.dispatch("batch " + quote_arg(json.dumps(
        ["set keep value 2", "rune new text extra", "set nosuch value 3"])))
    after = vc.export_state()
    check("a failed batch restores the slice from the memento",
          not r["ok"] and after["mantles"] == before_state["mantles"]
          and after["active"] == before_state["active"])
    check("and leaves the undo history exactly as it was",
          vc.dispatch("history")["data"] == before_hist)
    vc.dispatch("undo")
    check("and the earlier frames still undo correctly",
          active_runes(vc)[0]["content"].get("value", "") == "")
    ok = vc.dispatch("batch " + quote_arg(json.dumps(["set keep value 9"])))
    vc.dispatch("undo")
    check("a successful batch is still one undo frame",
          ok["ok"] and active_runes(vc)[0]["content"].get("value", "") == "")


def test_validate_duplicates_and_scale() -> None:
    doc = {"mantles": [{"id": "mantle_x", "name": "m", "domain": None,
                        "runes": [], "tags": {}, "layout": {"edges": []},
                        "rules": []}],
           "active": {"mantle": "m"}}
    runes = doc["mantles"][0]["runes"]
    for i, nm in enumerate(["a", "b", "a", "c", "b", "a"]):
        runes.append({"spirit": {"id": f"rune_{i:010x}", "name": nm},
                      "glyph": "text", "facets": {}, "tags": [], "content": {}})
    doc["mantles"][0]["layout"]["edges"] = [
        {"from": "a", "to": "zz", "relation": "", "weight": 1, "directed": True}]
    vc = VoidCore(doc)
    probs = vc.dispatch("validate")["data"]
    check("validate reports every later duplicate, once each",
          sorted(p for p in probs if p.startswith("duplicate")) ==
          ["duplicate name: a", "duplicate name: a", "duplicate name: b"],
          str(probs))
    check("and still reports a dangling endpoint", "dangling edge to 'zz'" in probs,
          str(probs))

    n = 50_000
    big = {"mantles": [{"id": "mantle_y", "name": "m", "domain": None,
                        "runes": [{"spirit": {"id": f"rune_{i:010x}", "name": f"r{i}"},
                                   "glyph": "text", "facets": {}, "tags": [],
                                   "content": {}} for i in range(n)],
                        "tags": {},
                        "layout": {"edges": [{"from": f"r{i}", "to": f"r{i + 1}",
                                              "relation": "", "weight": 1,
                                              "directed": True}
                                             for i in range(n - 1)]},
                        "rules": []}],
           "active": {"mantle": "m"}}
    vc = VoidCore(big)
    t = time.perf_counter()
    r = vc.dispatch("validate")
    ms = (time.perf_counter() - t) * 1000
    print(f"       validate at {n} runes + {n - 1} edges: {ms:.0f} ms")
    check("validate is linear: 5x10^4 runes in well under a second",
          r["ok"] and ms < 1000, f"{ms:.0f} ms")


def main() -> int:
    print(f"libvoidcore {VoidCore().version}\n")
    for t in (test_random_walk, test_inside_one_batch, test_name_equal_to_another_id,
              test_failed_batch_rolls_back_glyphs, test_failed_batch_with_memento,
              test_validate_duplicates_and_scale):
        t()
    print()
    if FAILS:
        print(f"FAILED ({len(FAILS)}): " + ", ".join(FAILS))
        return 1
    print("all ref-index checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
