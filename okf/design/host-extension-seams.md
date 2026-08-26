---
type: Design
title: Host extension seams
description: The blessed shape for host-registered dispatcher verbs and query predicates — one principle (host callbacks that ride the existing observable spine), two seams, both drift-test-clean.
tags: [status:planned, audience:dev, confidence:asserted]
timestamp: 2026-07-21T00:00:00Z
---

# Host extension seams — verb macros & pluggable predicates

> Prompted by two forward-looking asks routed Core-ward (Void Hormiga → the
> node-graph library → here, 2026-07-21): *"define a canvas action once, get both a
> gesture and a CLI verb"*, and *"let `ls`/[Scry](/concepts/scry.md) filter on a
> host-computed predicate (`--near`/`--radius`)."* Neither blocks anything today; the
> **shape** is what's owed, so that when the clients build, the human surface and the
> agent surface are one registration, not two implementations that drift.

## The one principle

Both asks are the **same move** the core already makes twice — the
[effect handler](/concepts/holiday.md) (`vc_set_effect_handler`, current) and the
planned [glyph](/concepts/glyph.md) `render`/`describe` callbacks:

> **The core owns the seam and the calling convention; the host owns the domain
> computation; the output rides the existing observable spine.**

This is exactly what keeps both asks on the safe side of
[the railguard](/design/what-voidcore-is-not.md) §4. The host parses the tokens and
does the distance math; the core never learns geometry or app semantics — it
orchestrates and structures, the host computes. Drift test: **passed**.

## Seam A — verb macros (the WRITE side, §1)

**Shape:** a host registers `{ verb-name, arg-spec, compile(argv) -> [command lines] }`.
The core parses per the arg-spec, calls the host's `compile`, and dispatches the
result **as a [`batch`](/concepts/dispatcher.md)**. That single decision buys every
property the ask wants, *by construction*:

- **atomic + one undo frame** — `batch` already is (SPEC §6);
- **attributed / logged / undoable / replayable** — the expansion is ordinary
  dispatcher commands on the normal mutation spine, so a volunteer's click and an
  agent's `place …` become the *same transcript entry*;
- **observable** — nothing escapes the log, because the handler emits commands, not
  effects.

**Expressible today?** The *semantics* already are: a host command bar that calls
`batch` with the compiled lines gets all of the above right now. What's missing is
**registration into the dispatcher's verb table**, so the verb is (a) discoverable via
`man`/`?`, (b) composable in [Voidscript](/concepts/voidscript.md), and (c) parsed
once by the core instead of reinvented per host. That registration is the only genuine
new surface — small, and squarely inside "one core, three faces."

**The critical constraint** (from [command architecture](/design/command-architecture.md)
§2, "the single most important distinction"): a verb macro emits **model mutations**,
so it is **pure → undoable → a `batch`**. It must **not** ride the effect seam, which
is for **effectful host I/O** (save/deploy/external write) and is *not* snapshot-undoable.
Pure-vs-effectful is the line that decides which seam a host op belongs to.

**Data-side twin:** planned Voidscript `def`/functions (roadmap) let a *mantle* define
`place` as a script macro with no C registration — automatically discoverable and
composable. Verb macros are the FFI/host-language counterpart of that same idea; they
should share the arg-spec and expansion model so the two paths don't diverge.

## Seam B — pluggable query predicates (the READ side, §2)

**Shape:** a host registers `name -> predicate(rune, args) -> bool`; the
[Scry](/concepts/scry.md) `where` evaluator calls it as a host callback during
filtering, alongside the [tag grammar](/concepts/tag-system.md) — so it **composes**
(`ls --near "45.5,-122.6" --radius 500 AND --tag "contact"`).

Why the tag evaluator can't already do this: `vc_tag_match(expr, tags_json)` is a pure
function over a **bare bag of tags** — deliberately stateless, no rune facets. A spatial
test is a computation over the rune's `geo` **facet**, not tag-membership; it is a
predicate over the *rune*, not over its tag bag. Genuinely a different evaluation shape,
which is why it can't be faked in the grammar. This seam is the natural sibling of "one
evaluator over the FFI": **one evaluator, pluggable predicates.**

**Expressible today?** *Partially.* The escape hatch exists — `effect query "<expr>"`
routes a whole query to a host and returns runes as `data`, so Hormiga could push a
spatial index into a [holiday](/concepts/holiday.md) and filter host-side **now**. What
that *cannot* do is **compose inside `where`**: it's a whole-query replacement reachable
only as an effect, not a filter term an agent can `AND` with the tag grammar over the
core's own `ls`. The composable predicate is the blessed new thing.

**Boundary:** the core owns only registration + the calling convention + composition
into `where`; the host owns the distance math and the lat/lon types. Same quarantine as
Seam A and the view-side library keeps — the core never grows a geo type.

## Why record it now

Nothing is being built here — this is the **stance**, pinned so the clients build
against one shape. Consistent with how this collaboration has evolved the contract
(daily-use findings → pinned decisions). When a real second consumer appears for either
seam, it graduates from this note to `SPEC.md` and the roadmap moves it to `log.md`.
