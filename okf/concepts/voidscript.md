---
type: Concept
title: Voidscript
description: A small terminal-complete language over the dispatcher; every non-control line is a dispatcher command.
resource: core/src/scripts/voidscript.c
tags: [status:current, audience:library, audience:dev, confidence:asserted]
timestamp: 2026-08-28T00:00:00Z
---

**Voidscript** is a scripting language whose every non-control statement is a
[dispatcher](/concepts/dispatcher.md) command. The simplest script is a list of
commands; the language adds branching, loops, and error handling so routines (and
agents) can codify complex edits.

# Built (subset)

`let` + `$var`/`${var}`/`$1..`/`$@`/`$?`, `$(cmd)` capture, `if/elif/else`, `while`,
`repeat`, `foreach v in (cmd)`, `break`/`continue`, `return`, `halt`, `assert`,
operators. Runnable via `script`.

# A value is data, never syntax

A script is a **transcript**, and a transcript is how someone else's text reaches
the model: a submission from a stranger, a dataset harvested from a PDF, an agent's
proposed run. So the interaction between [§6.1
quoting](/concepts/dispatcher.md) and Voidscript's own syntax is normative (SPEC
§8.1), and it comes down to three rules:

- **Statement boundaries are honored only outside quoted runs.** A newline, `;`,
  `{` or `}` inside a quoted argument is content. The statement reader uses the
  *same* quote scanner as the argv tokenizer — one automaton, no second opinion.
- **Single quotes suppress expansion.** `$var` and `$(cmd)` are literal inside a
  single-quoted run, exactly as `\n` and `\cY` are. A transcript built by correctly
  quoting a stranger's text must not run what that text happens to contain.
- **An expansion is exactly one argument** — never re-scanned for quotes,
  separators or flags, and an empty one is an explicit empty argument rather than a
  disappearance.

None of this held before 0.2.7, and the reason is worth keeping: the statement
reader, the condition lexer and the interpolator each carried their **own** quote
tracking, and none implemented §6.1's `\'` escape. A value spelled exactly the way
the SPEC tells hosts to spell it closed its quoted run in the reader but not in the
tokenizer, and everything after the next newline ran as commands — `ok: true`,
canary breached. Conformance case `13-transcript-safety.vs` pins it, and the fix
was structural: delete the other scanners. The **number** of tokenizers was the
bug; the rules of any one of them were a symptom.

# A CR is a line terminator, not content

`\r\n`, `\n` and a bare `\r` all end a statement outside a quoted run; inside one a
CR is data like any other byte (SPEC §8, 0.2.9). This is the same lesson in a
smaller costume, and it survived two more releases than the one above because it
had no canary. Until 0.2.9 the statement reader treated CR as an ordinary
character while `skip_sep` already treated it as a separator — two readers, one
disagreement — so a CRLF-authored script returned `"ok\r"` from `return ok` and
made `assert a == a` false, reporting **neither**. Numeric comparison coerced
through it, so the same script passed or failed depending on which *kind* of value
a line happened to compare: it read as an intermittent logic bug, not a newline
problem. Void Unity found it from a Windows host (2026-08-27), where CRLF is
simply what an editor, a Unity `TextAsset` or a designer's clipboard hands you.

The second half is why no test caught it: `conformance/run.py` read cases in
Python text mode, whose universal-newline translation rewrote CRLF to LF before
the library saw a byte — so the suite could not observe the behavior *by
construction*, while `14-journal.vs` sat in the repository with CRLF, green here
and red for the first host that read it faithfully. **A suite that normalizes its
own inputs is not testing what a host will be handed** (now normative, SPEC §11).
Case `15-crlf.vs` is stored with CRLF on purpose and marked `-text` in
`.gitattributes` so no checkout can quietly repair it.

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
