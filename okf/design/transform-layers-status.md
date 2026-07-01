---
type: Design
title: Transform layers — status update
description: Running status of the Reduce/Temper/Scry build-out and its follow-ups.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Void Core — status update for anyone building on it (2026-06-28)

> A broadcast, not a request. If you're building an app on Void Core, here's what now
> exists and how to use it. No reply needed — just build against it. (Some of this was in
> the earlier handoff; it's repeated here so this stands alone.)

## TL;DR

The three transformation layers — **Reduce / Temper / Scry** — plus **materialize** are
built, tested, exposed as dispatcher verbs, **authorable as data**, and reachable through a
single Python surface (`from voidcore import ...`). Owned state stays pure model; all real
I/O goes through one **effect-handler** boundary, now bound in Python. Nothing you were
relying on has been removed; this is all additive.

Install once, editable, and updates transfer with no copying:

    pip install -e <path-to>/VoidCore        # then:  from voidcore import VoidCore, Dispatcher, ...

---

## The model in one breath

A **rune** is the atomic editable unit (a spirit id+name, a glyph/type, tags, content,
relations). A **mantle** is a set of runes + a layout graph (edges) + rules. A **holiday**
is any external system you don't own (DB, file host, API, knowledge bundle), reached over a
protocol. The **dispatcher** is the one command surface (`{ok, lines, data}` for every
verb); every mutation is undoable and dirty-tracked.

## The three transformation verbs (nouns are data; these are the transformations)

- **Scry** — the **read** side: pure projection/resolution. Derive views from owned state
  (+ an optional holiday snapshot + a context) without mutating anything. Same inputs ⇒
  identical output, so live-preview == final render == archived send.
- **Temper** — **normalization**: a pure, **idempotent** pass that cleans owned state to a
  canonical form after an action (derived-field defaults like `thumb = images[0]`, dedupe,
  tag normalization). `temper(temper(x)) == temper(x)`.
- **Reduce** — the interaction-net **executor** (graph rewriter): fire a mantle's rules on
  active pairs until **normal form**, producing a *derived* mantle (source untouched). Pure
  `net -> net`, **strongly confluent** on the restricted glyph-pair rule form (verified on
  the full γδε interaction-combinator system under randomized schedules). Feedback cycles
  are preserved (only active pairs reduce; you can also mark agents *opaque*). A `max_steps`
  guard catches non-termination.
- **materialize** — the *one* explicit, undoable action that **freezes** a resolved
  projection (e.g. holiday-snapshot data) into owned state. Distinct from Reduce's transient
  reference-expansion. Optional `stamp=` records **provenance** (a stable snapshot id) so an
  archive proves *what* it captured.

Hard rules all four obey: **pure** (no I/O / clock / RNG), **functional** (never mutate the
source), **identity is the default** (no rules ⇒ a value transforms to itself), and **no
effects ever fire from inside them** — effects live only at the holiday boundary.

## What's new since the layers first landed

**1. The verbs are on the dispatcher (a drop-in seam).**
`Dispatcher(vc)` is a **superset** of `vc.dispatch` — it adds the transform verbs and
delegates every other command to the C core unchanged:

    d = Dispatcher(vc)
    d.dispatch('scry "status:active"')          # filter -> matching rune names
    d.dispatch("scry --select recent")          # a registered/loaded projection -> views
    d.dispatch("temper alpha")                   # normalize a rune (or all), write-back
    d.dispatch("materialize beta hits=7 --stamp frozen")
    d.dispatch("reduce --into nf [--commit]")    # rewrite the mantle's net to normal form
    d.dispatch("ls")                             # delegated to the core, identical behavior

Tag-expression note for `scry`/`ls --tag`: the grammar is **`AND` / `OR` / `NOT`** (with
`&&` `||` `!`, and adjacency = implicit AND). So "require A and B, exclude C" is
`A AND B AND NOT C` (the `+req / -excl` sigils are the **`tag` verb's** mutation syntax, not
the filter syntax). One evaluator, conformance-tested against the C core.

**2. Rules and projections can be authored as DATA (not just code).**
`voidcore.spec` compiles JSON into the same tested objects, and the specs ride in the state
document under `config.transform`, loaded with `Dispatcher.load_from_config()`:

    {"temper":    [{"rule":"member_or_default","target":"thumb","source":"images"},
                   {"rule":"dedupe","field":"images"}, {"rule":"normalize_tags"}],
     "selectors": {"recent": {"where":"status:active","sort":"date","limit":10}},
     "reduce":    {"signatures": {"con":2,"dup":2,"era":0},
                   "rules": [{"glyphs":["con","con"],"rule":"annihilate"},
                             {"glyphs":["con","dup"],"rule":"commute"}]}}

So a mantle can carry its own normalization rules, named projections, and rewrite rules —
editable without code.

**3. A Lens for bidirectional mappings (and the round-trip law).**
A persistence mapping (e.g. a record⇄rune mapping) should be **one** tested projection, not
re-implemented for read / write / persist. A `Lens` bundles `forward` + `backward` with the
**round-trip law** `unscry(scry(x)) == x` (`lens.check(samples)`), which makes silent
data-loss structurally catchable. `LocalJsonHoliday.lens()` gives you one for free.

**4. temper-on-write (opt-in).**
`Dispatcher(vc).temper_on_write(True)` runs your registered Temper pass automatically after
every mutating verb, re-normalizing the affected rune(s) — so invariants hold even for
**raw** dispatcher edits (the surface you may expose to users), not just your high-level
methods.

**5. Atomic single-frame undo.**
A multi-write transform pass (a multi-rune `temper`/`materialize`, a `reduce --commit`, a
temper-on-write repair) is applied as **one** undo frame via the core's `batch` verb (one
snapshot, rolled back on any failure). One author action ⇒ one undo. (The tokenizer was made
dynamic so large batches aren't truncated.)

**6. The effect handler (holiday boundary) is bound in Python.**

    vc.set_effect_handler(fn)        # fn(op, args) -> dict | str | None

It receives `save` (args = the full state document), `deploy`/`build`/`preview`
(args = `{"args":[...]}`), and the **new generic `effect <op> [args...]` verb**, which routes
*any* host op through the same seam and returns its result as `data`. That makes read
effects reachable — e.g. a holiday query:

    vc.dispatch('effect query "flier AND event"')   # -> data = your store's matching runes

Memory is handled for you (returns are copied with the library's allocator, so the core
frees them safely across the FFI).

## The import surface (Python)

    from voidcore import (
        VoidCore, Dispatcher,
        # scry
        scry, Selector, Context, dedupe_by, tag_match,
        materialize, provenance, check_roundtrip, Lens,
        # temper
        Temper, dedupe, member_or_default, default_content, default_tag,
        single_tag, normalize_tags,
        # reduce
        Reducer, Net, Agent, annihilate, commute, expand, to_net, from_net,
        # data-authored specs
        temper_from_spec, selector_from_spec, reducer_from_spec,
        # holidays
        LocalJsonHoliday, RecordSchema, holiday,
    )

Everything pure-Python imports with no third-party deps; heavier holidays (e.g. MeshDB) are
lazy via `holiday("...")`.

## What's intentionally NOT done (so you don't wait on it)

- **General rule LHS** for Reduce (tag-expression / sub-pattern matching) — deferred; the
  restricted glyph-pair form is what carries the confluence guarantee. Cross-rune /
  shared-target rewrites (e.g. "route many sources to one bus") aren't expressible yet; build
  those by hand for now.
- **Data-form `expand`** — `expand` needs a custom build function, so it stays code-registered
  (`annihilate`/`commute` are the data-authorable rule kinds).
- **Wiring your real stores to the effect handler** — that's *your* integration step; the
  seam and the `effect` verb are ready whenever you start.
- **Inter-application communication** (apps talking to each other on top of the OKF) — a
  promising idea, deliberately shelved for later.

## Where to read more

- `SPEC.md` §6–§7 (dispatcher + verbs, incl. the transform verbs `[seam]`), §9 (effect seam).
- The Void Core OKF bundle (`okf/`) — concepts for Reduce / Temper / Scry / Dispatcher /
  Holiday / Interaction nets; `okf/roadmap.md` for what's done vs planned; `okf/log.md` for
  the change history. Browse it via `python holidays/okf serve` (a wiki-style viewer).
- `notes/reducer.md` — the design + the resolved forks behind the three layers.
