---
type: Component
title: Python binding
description: The ctypes binding wrapping the libvoidcore.dll C ABI; first binding target.
resource: bindings/python/voidcore.py
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-06-28T00:00:00Z
---

The first language binding for the [C core](/components/c-core.md): a thin `ctypes`
wrapper where everything crosses as JSON strings, so the host never touches C memory
directly. Exercised by the [MeshDB holiday](/components/meshdb-holiday.md) smoke test.

# Surface

`VoidCore(state).dispatch(cmd) -> {ok,lines,data}`, `export_state()`,
`register_glyph()`, **`set_effect_handler(fn)`**, context-manager `close()`.

# Effect handler

`vc_set_effect_handler` is now **bound**: `set_effect_handler(fn)` registers a Python
callable `fn(op, args) -> dict|str|None` as the [holiday](/concepts/holiday.md) boundary.
It receives `save`/`deploy`/`build`/`preview` and the generic `effect <op>` verb; the
return is marshalled back through `vc_alloc_str` so the core frees it safely across the FFI
(verified under repeated calls). This unblocks routing `save` and read effects (e.g. a
holiday query) through host code.

# Status

`current` — dispatch/state/glyph/effect-handler paths, binding smoke + `effect_test.py`
green.
