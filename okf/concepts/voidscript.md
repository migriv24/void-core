---
type: Concept
title: Voidscript
description: A small terminal-complete language over the dispatcher; every non-control line is a dispatcher command.
resource: core/src/scripts/voidscript.c
tags: [status:current, audience:library, audience:dev, confidence:asserted]
timestamp: 2026-06-18T00:00:00Z
---

**Voidscript** is a scripting language whose every non-control statement is a
[dispatcher](/concepts/dispatcher.md) command. The simplest script is a list of
commands; the language adds branching, loops, and error handling so routines (and
agents) can codify complex edits.

# Built (subset)

`let` + `$var`/`${var}`/`$1..`/`$@`/`$?`, `$(cmd)` capture, `if/elif/else`, `while`,
`repeat`, `foreach v in (cmd)`, `break`/`continue`, `return`, `halt`, `assert`,
operators. Runnable via `script`.

# Examples

Every non-control line is a [dispatcher](/concepts/dispatcher.md) command; the
constructs just compose them. All examples use the built subset.

A batch edit over a [tag](/concepts/tag-system.md) query — `foreach` iterates a
command's `data`/lines:

```voidscript
# tag every featured rune as reviewed
foreach r in (ls --tag "featured") {
  tag $r add reviewed
}
```

Guard a mutation on its result (`$?` = last command's ok):

```voidscript
set hero.title "Welcome"
if $? {
  echo "title updated"
} else {
  halt 1
}
```

Capture structured output, then branch on it:

```voidscript
let s = $(status --json)
if ${s.dirty} > 0 {
  echo "unsaved changes present"
}
```

**`assert` as the seed of a test engine.** A script of assertions over the
dispatcher is, in effect, a unit test of a mantle's invariants — run it after an
edit (or in CI) and a false assertion `halt 1`s:

```voidscript
# invariant: no rune may sit in two lifecycle states at once
foreach r in (ls) {
  let n = $(get $r --json)
  assert !( ${n.tags} contains "status:draft" && ${n.tags} contains "status:published" )
}
```

This `assert`-over-dispatcher shape is the embryo of a Void Core **unit-testing**
direction (see [logging & debug](/concepts/logging-debug.md)) — the same scripts
that automate edits can codify the checks that keep a mantle honest.

# Planned

`def`/functions, `try`/`catch`, `on error`, `include`, `call`, `wait`, `prompt`.

# Status

`current` (the subset above, in the [C core](/components/c-core.md)); the advanced
constructs are `planned`. See `SPEC.md` §8.
