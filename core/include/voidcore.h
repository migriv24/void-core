/*
 * voidcore.h — the public C ABI for Void Core.
 *
 * This header is intentionally pure C (no C++ types in any signature) so that the
 * library is callable from any language with a C FFI (Python ctypes, Node N-API,
 * Rust, Godot GDExtension, ...). The *implementation* may grow richer internally;
 * this boundary never does. Data crosses as JSON strings.
 *
 * Memory rule: any `char*` returned by this library is owned by the caller and
 * MUST be released with vc_free_str(). Handles (VC_Manager*) are released with
 * vc_destroy(). Strings returned by vc_version() are static — do NOT free them.
 */
#ifndef VOIDCORE_H
#define VOIDCORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Export/import decoration is only meaningful for a shared-library build on
 * Windows. A single-binary build (define neither VC_DLL nor VC_BUILD_DLL) gets a
 * plain declaration. */
#if defined(_WIN32) && defined(VC_DLL)
#  if defined(VC_BUILD_DLL)
#    define VC_API __declspec(dllexport)
#  else
#    define VC_API __declspec(dllimport)
#  endif
#else
#  define VC_API
#endif

/* Opaque manager handle. */
typedef struct VC_Manager VC_Manager;

/* Host callbacks (the holiday boundary — all real I/O lives in the host).
 *  - VC_LogFn:    receives each log line live (host decides where it goes).
 *  - VC_EffectFn: handles effectful ops the core can't do itself (save to a real
 *    backend, deploy, build, preview). Receives the op name + a JSON payload and
 *    returns a malloc'd JSON result string (or NULL); the core frees it. */
typedef void (*VC_LogFn)(const char *level, const char *op, const char *msg, void *user);
typedef char *(*VC_EffectFn)(const char *op, const char *args_json, void *user);

/* Create a manager from a state-document JSON string (SPEC §2).
 * Passing NULL or a malformed/non-object document yields the empty state.
 * Returns NULL only on allocation failure. */
VC_API VC_Manager *vc_create(const char *state_json);

/* Dispatch one command line (SPEC §6). Returns a heap JSON string of the form
 * {"ok":<bool>,"lines":[...],"data":<any|null>}. Caller frees with vc_free_str.
 * Returns NULL only if m is NULL. */
VC_API char *vc_dispatch(VC_Manager *m, const char *command);

/* Serialize the full state document (SPEC §2) as a JSON string. Caller frees
 * with vc_free_str. */
VC_API char *vc_export_state(VC_Manager *m);

/* Register (or override) a glyph from a JSON descriptor, e.g.
 *   {"glyph":"dialogue","label":"Dialogue line","editor":"form",
 *    "fields":["speaker","text","expression"]}
 * Returns 1 on success, 0 on bad input. Glyphs are host-supplied app config and
 * are NOT part of the exported state; re-register them after each vc_create. */
VC_API int vc_register_glyph(VC_Manager *m, const char *glyph_json);

/* Register the host log sink (receives every log line live). */
VC_API void vc_set_log_sink(VC_Manager *m, VC_LogFn fn, void *user);

/* Register the host effect handler (save/deploy/build/preview reach the world
 * through this). The core does its own model-side work (e.g. save snapshots the
 * baseline) and calls this for the external side. */
VC_API void vc_set_effect_handler(VC_Manager *m, VC_EffectFn fn, void *user);

/* Allocate a library-owned, NUL-terminated copy of `s` (NULL -> NULL). For host
 * effect handlers (VC_EffectFn) to build their return value: the core frees that
 * string with the library's own allocator, so it must be allocated by this one —
 * use this instead of the host language's allocator to avoid a cross-CRT free. */
VC_API char *vc_alloc_str(const char *s);

/* Release a string returned by this library. */
VC_API void vc_free_str(char *s);

/* Destroy a manager and everything it owns. */
VC_API void vc_destroy(VC_Manager *m);

/* Library version (static string, do not free). */
VC_API const char *vc_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VOIDCORE_H */
