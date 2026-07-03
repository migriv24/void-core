---
type: Concept
title: Holiday
description: The protocol interface to an external system the application does not own — an API, cloud DB, file host, or knowledge bundle.
resource: SPEC.md
tags: [status:current, audience:library, audience:dev, confidence:asserted, foundation]
timestamp: 2026-07-01T00:00:00Z
---

A **holiday** is how Void Core reaches a system it does *not* own — it is "away" from
the pure core. All real I/O is a holiday: the core does no file/network/stdout work
itself. Mostly conceived right now as a **Backend-as-a-Service** interface, but the
pattern is general (data, asset, compute, output, LLM, and *knowledge* backends).

# Interface

```
query(tagExpr)  -> [row | rune]     get(ref) -> payload
insert(row)     -> ref              describe() -> { capabilities, kind, status }
```

# What is built vs planned

- **Current**: the **effect-handler seam** (`vc_set_effect_handler`), now **bound in the
  Python binding** (`VoidCore.set_effect_handler(fn)`), through which `save`/`deploy`/
  `build`/`preview` route to host I/O. A generic **`effect <op> [args...]`** verb routes
  *arbitrary* host ops through the same seam and returns their result as `data` — so the
  `query(tagExpr) -> [rune]` interface above is reachable today (e.g. Hormiga's
  "holiday query → tagged rune collection": `effect query "<expr>"`). The cross-allocator
  return is handled by `vc_alloc_str` (the host builds its result with the library's
  allocator, the core frees it).
- **Planned**: a tagged **holiday registry** — many holidays, selected by tag/
  capability with fallback chains. Concrete first holidays:
  [MeshDB](/components/meshdb-holiday.md) (data BaaS) and the
  [OKF engine](/components/okf-engine.md) (knowledge).

A holiday is reached over a protocol, so the backend's language is irrelevant — a
Rust graph DB is reached over Bolt, not linked in. This is what kept the
[C core](/components/c-core.md) from needing a rewrite.

# Status

`current` (the seam); the registry is `planned`. See `SPEC.md` §10.1.
