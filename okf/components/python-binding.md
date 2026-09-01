---
type: Component
title: Python binding
description: The ctypes binding wrapping the libvoidcore.dll C ABI; first binding target.
resource: bindings/python/voidcore.py
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-28T00:00:00Z
---

The first language binding for the [C core](/components/c-core.md): a thin `ctypes`
wrapper where everything crosses as JSON strings, so the host never touches C memory
directly. Exercised by the [MeshDB holiday](/components/meshdb-holiday.md) smoke test.

# Surface

`VoidCore(state).dispatch(cmd) -> {ok,lines,data}`, `export_state()`,
`register_glyph()`, **`set_effect_handler(fn)`**, context-manager `close()`.

# The pure surface (no engine, no build)

`quote_arg`, `split_args`, `split_transcript`, `Lens`/`pipeline`/`check_roundtrip`,
and the Scry/Temper/Reduce layers import with **no compiled library at all** —
`ctypes.CDLL` is called inside `VoidCore.__init__`, never at module scope. That is a
**guarantee, not an accident**: `voidcore/pure_import_test.py` poisons `CDLL` and
still exercises the surface, so moving a load to module scope fails a test rather
than silently breaking an archival tool (Void Reyna's ask, 0.2.6).

The [§6.1 codec](/concepts/dispatcher.md) lives here in both forms — the pure
Python functions above, and `arg_quote` / `argv_split` / `transcript_split` methods
that call the C implementation, which is how `voidcore/codec_test.py` checks the two
against each other. The C-side three are bound **leniently**, so this binding still
loads against a library older than 0.2.7 and says so if you reach for them.

# The command journal

`set_journal(bool)`, `journal()`, `journal_clear()` wrap the §6.2 record. Bound
**leniently** like the codec, so this binding still loads against a library older
than 0.2.8 and says so if you reach for them. `journal()` returns the parsed
entries; a consumer building a replayable or transmissible history keeps only the
`pure` ones.

# Undo control

`set_undo(bool)` and `set_undo_depth(int)` wrap the §6 switch. Bound **leniently**
like the codec and the journal, so this binding still loads against a library
older than 0.2.9 and says so if you reach for them. Undo is on by default; a host
whose `mantles` holds live instances rather than a design should turn it off and
accept that `undo` fails, which is a bargain only the host can strike.
`bindings/python/undo_control_test.py` carries the contract *and* the benchmark
that motivated it.

# Effect handler

`vc_set_effect_handler` is now **bound**: `set_effect_handler(fn)` registers a Python
callable `fn(op, args) -> dict|str|None` as the [holiday](/concepts/holiday.md) boundary.
It receives `save`/`deploy`/`build`/`preview` and the generic `effect <op>` verb; the
return is marshalled back through `vc_alloc_str` so the core frees it safely across the FFI
(verified under repeated calls). This unblocks routing `save` and read effects (e.g. a
holiday query) through host code.

# Status

`current` — dispatch/state/glyph/effect-handler paths, the pure surface, and the
§6.1 codec; binding smoke, `effect_test.py`, `attribution_test.py`,
`pure_import_test.py`, `version_test.py` and `codec_test.py` green.
