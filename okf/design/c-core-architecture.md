---
type: Design
title: Void Core as a C core with FFI bindings
description: The rationale for one C library + thin bindings; what is in the pure core vs at the holiday boundary.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-01T00:00:00Z
---

# Void Core as a C Core with FFI Bindings

> **Log / outline — still brainstorming.** Captures the decision to implement
> Void Core as a **C library** with foreign-function-interface (FFI) bindings, so
> *any* host language can use it. Pairs with the railguard
> ([what-voidcore-is-not.md](/design/what-voidcore-is-not.md)): a C *library* an app links
> against is an overlay; it is still **not** a language or runtime you write your
> app in.

---

## 1. Why C

Any language can call C. C has a stable ABI and an FFI from essentially
everywhere (Python `ctypes`/`cffi`, Node N-API, WASM, JVM JNI, Rust `extern "C"`,
Godot GDExtension, …). So instead of re-implementing Void Core per language, we
write **one core in C** and ship **thin bindings**.

### Supersedes the earlier plan
This **replaces** the "Python port + JS reference impl = two implementations kept
in sync by a spec" idea (`Hormiga/VOIDCORE_INTEGRATION.md` §4, the standalone
memory). New shape:

- **One implementation** — the C core — is the single source of truth.
- **`SPEC.md` is the contract the C core implements** (and that bindings expose),
  not a referee between rival codebases.
- The **JS reference impl becomes a prototype / oracle**: it proved the model and
  still runs BiologyManager. Keep it working until `C core + JS binding` can
  replace it; use it as a conformance oracle (`SPEC.md §11`). Do **not** rewrite
  everything at once.

> Trade-off to stay honest about: C is harder and slower to write than the JS
> prototype, and FFI memory management is fiddly. The win (write once, bind many)
> only pays off because Void Core is meant to underlie *many* apps in *many*
> languages. If it were one app, JS would be fine. It isn't — so C.

---

## 2. What is IN the core (pure C, zero I/O)

The core is **pure**: data in, data out, no side effects. This is what makes it
bind-anywhere *and* keeps it on the right side of the railguard.

- The **data model**: spirit, rune, glyph, mantle, domain, holiday, binding,
  tags (`SPEC.md §3`). Serializable to/from the state document (`SPEC.md §2`).
- The **dispatcher**: parse a command → mutate model → return `{ok, lines, data}`
  (`SPEC.md §6`). Undo/redo + dirty-tracking as in-memory structures.
- The **tag engine**: the filter-expression parser/evaluator (`SPEC.md §5`).
- The **Voidscript interpreter** (`SPEC.md §8`) — so every host shares one DSL.
- Undo stack, log *buffer* (as data — see below).

## 3. What is OUT of the core (host-provided, at the holiday boundary)

**All I/O is a holiday.** The core never reads a file, opens a socket, or prints
to a terminal. The host provides those through callbacks / adapters:

- **Persistence** — reading/writing the state document and the real backend
  (files, DB) is the **adapter** (`SPEC.md §9`), implemented in the host language.
- **The log sink** — the core emits log *records*; the host decides where they go
  (file, stderr, an Electron pane).
- **Holidays** — network, external APIs, deploy. Host-implemented, registered into
  the core as callbacks (`SPEC.md §10.1`).
- **The CLI front-end itself** — see §4.

## 4. The CLI is a holiday (the catch you spotted)

> *"how exactly to display and how the user even interacts with the cli (already
> running into a holiday issue haha)"* — yes, exactly.

The dispatcher is pure: it takes a command string and returns a result struct. It
does **not** render, read keypresses, or own the REPL loop. **Rendering, input,
and the read-eval-print loop are a host adapter — i.e. a holiday** (the terminal
and the human are systems Void Core does not control; it *broadcasts* output to
them and *receives* input from them). So:

- **Core provides:** command *semantics* (`dispatch(cmd) -> result`) + `describe`
  output as structured data.
- **Host provides:** how that result is *displayed* and how the user *drives* it —
  a plain terminal REPL, a fancy TUI, an Electron panel, a web textbox. Each app
  ships its own CLI front-end over the same pure dispatcher.

This is the same separation as BiologyManager (the server hosts the GUI; the core
hosts dispatch) — now stated as a principle. It also matches how PEL and NeLA keep
I/O at the edge (PEL's pure core + LLM hooks; NeLA's linear `IOToken` threading).

## 5. The FFI surface (conceptual sketch — not final)

A small `extern "C"` ABI, plus **callback registration** so hosts implement
glyphs/adapters/holidays *in their own language*:

```c
VC_Manager* vc_create(const char* state_json);
char*       vc_dispatch(VC_Manager*, const char* command);  // returns result JSON
void        vc_free_str(char*);
void        vc_destroy(VC_Manager*);

// host plugs its language in via callbacks:
void vc_register_glyph  (VC_Manager*, const char* name, VC_DescribeFn, VC_NewContentFn);
void vc_register_holiday(VC_Manager*, const char* name, VC_QueryFn, VC_InsertFn, ...);
void vc_set_adapter     (VC_Manager*, VC_SaveFn /* host writes the real backend */);
void vc_set_log_sink    (VC_Manager*, VC_LogFn);
```

- Data crosses the boundary as **JSON strings** (simple, language-neutral) at
  first; optimize to structs later if needed.
- A glyph's `describe`/`render`, an adapter's `save`, a holiday's `query` are
  **callbacks up into the host** — that's how a C++ game or a Python app supplies
  its domain logic without the core knowing the host language.

## 6. Packaging (the "npm package / widget" intuition)

- **`void-core`** — the C library (source + build).
- **`void-core` (npm)** — JS binding via **N-API** (Node/Electron) or **WASM**
  (browser). First target, for website managers.
- **`voidcore` (pip)** — Python binding via **cffi/ctypes**. Second target.
- **Per-host starter scaffolds** — the "widget/leaflet" idea: a small starter that
  wires the core + a default CLI front-end + a place to register domain glyphs.
  An app does `npm install void-core` / `pip install voidcore`, registers its
  domain, ships.

---

## 7. Open questions (to extend)
- **Memory across FFI** — who owns strings/handles; ref-counting vs explicit free.
- **Async holidays** — network/LLM calls are async; the pure-sync dispatcher needs
  a story for awaiting a holiday (callback returns a promise/future to the host?).
- **WASM vs native in the browser** — website managers may run in-browser; WASM
  build of the core?
- **Voidscript-in-C now or later?** Porting the interpreter to C is real work;
  could keep it in the JS prototype until the model layer is solid in C.
- **Build system / cross-compilation** — one C core that builds for win/mac/linux
  + WASM. (Hormiga already bundles via PyInstaller; a `.dll`/`.so`/`.dylib` rides
  along.)
