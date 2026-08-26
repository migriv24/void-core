---
type: Concept
title: Logging & debug
description: The logging spine — leveled, op-tagged, buffered log records the host sinks anywhere — plus the debug/inspection surface that grows from it toward Voidscript-driven testing.
resource: core/src/util/log.c
tags: [status:current, audience:library, audience:dev, confidence:asserted]
timestamp: 2026-08-25T00:00:00Z
---

Void Core keeps a **logging spine** as a first-class part of the engine, not an
afterthought. Every meaningful action can emit a **leveled, op-tagged log record**;
the core buffers records as data and the host decides where they go — staying on the
right side of the railguard (the core does no I/O; see
[what Void Core is NOT](/design/what-voidcore-is-not.md)). Logging matters for two
reasons: it is what makes the [CLI](/concepts/dispatcher.md) legible to a human and an
agent, and it is the substrate that debugging and **testing** build on.

# Built (current)

- **C core** (`core/src/util/log.c`): `vc_log(m, level, op, fmt, …)` appends a record;
  `vc_log_buffer(m)` exposes the buffer as data; **`vc_set_log_sink(m, fn, user)`** lets
  the host receive every line live (file, stderr, a UI pane) — a [holiday](/concepts/holiday.md)
  boundary. A `log` [dispatcher](/concepts/dispatcher.md) verb surfaces the buffer.
- **No length cap on a record.** `vc_log` and the result-line builders format into a
  heap buffer sized to the message. They used to be 1024-byte stack arrays, which cut
  a long value *mid-UTF-8 sequence* — so a command that had succeeded came back with
  invalid bytes in its `lines`, and the host's JSON parse threw. About 500 characters
  of any accented script was enough. A fixed buffer on a string path turns *long*
  content into *invalid* content, which fails in the parser rather than in the data.
- **JS oracle** (`src/log/logger.js`): the same shape — timestamped/leveled lines,
  a ring buffer, `tail(n, level)`, level filtering, and an event stream for live tailing.

Records carry `{ ts, level, op, message }`, so logs are **queryable data**, not just
text — an agent can filter by op or level the same way it filters [tags](/concepts/tag-system.md).

# Debug & testing (planned)

The debug surface grows from the same spine:

- **Richer inspection verbs** — structured `describe`/`status --json` already expose
  state; a dedicated debug/trace mode (record which verbs fired, with timings) is `planned`.
- **A Voidscript test engine** — `assert`-over-dispatcher scripts
  ([Voidscript examples](/concepts/voidscript.md)) are the embryo: the same language that
  automates edits can codify invariant checks and run them after an edit or in CI. Formalizing
  this into a first-class testing/logging tie-in is `planned`.

# Status

`current` for the logging spine (C core + JS oracle, `SPEC.md` §9); `planned` for the
richer debug/trace tooling and the Voidscript-driven test engine.
