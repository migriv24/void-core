---
type: Component
title: C core
description: The C implementation of Void Core, exposing a pure C ABI as a self-contained libvoidcore.dll.
resource: core/README.md
tags: [status:current, audience:library, audience:dev, confidence:asserted]
timestamp: 2026-08-30T00:00:00Z
---

The current implementation of Void Core: ~3300 lines of hand-written C over vendored
cJSON, exposing a pure **C ABI** (`core/include/voidcore.h`) so any language can bind
to it. Builds to a self-contained `libvoidcore.dll` (CMake + Ninja, MSYS2 UCRT64
GCC). Supersedes the old JS prototype in `src/` (now a conformance oracle).

# All five parts implemented

- [Data model](/concepts/rune.md): spirit/rune/[glyph](/concepts/glyph.md)/[mantle](/concepts/mantle.md)/[domain](/concepts/domain.md)/binding over cJSON.
- [Dispatcher](/concepts/dispatcher.md): one router, `{ok,lines,data}`, undo/redo, dirty-tracking, atomic batch, the SPEC §7.1 POSIX alias surface (argument-aware desugarings in `args.c`).
- [Tag system](/concepts/tag-system.md): membership, axes, filter grammar (also exported standalone as `vc_tag_match` for hosts), weighted tag graph.
- [Voidscript](/concepts/voidscript.md): the implemented subset.
- Logging spine + the effect-handler ([holiday](/concepts/holiday.md)) seam.

# Design stance

Exception-free, NULL-tolerant boundary (bad host input never crashes the lib). The
core does no I/O — `vc_dispatch(cmd) -> result JSON` is the whole surface.

# Status

`current`, **v0.2.0**. C smoke + Python binding smoke + the `conformance/` suite
(8/8, SPEC §11) green as of 2026-07-03. CI (`.github/workflows/ci.yml`) builds and
conformance-tests win/mac/linux; a `v*` tag publishes the three prebuilt libraries
as a GitHub Release for downstream vendoring. The dispatch is single-threaded by
design — hosts serialize per manager (SPEC §6). Not in the C library by design: the
[interaction-net](/concepts/interaction-nets.md) reducer lives at the Python seam
([Reduce](/concepts/reduce.md)); still deferred here: glyph host callbacks over FFI,
advanced Voidscript. Design: [C core with FFI bindings](/design/c-core-architecture.md).
