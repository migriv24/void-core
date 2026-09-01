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
 *
 * Threading rule (SPEC §6): a VC_Manager is NOT thread-safe — it holds one
 * mutable state document and unsynchronized undo/redo stacks. Serialize every
 * call that takes the same manager (vc_dispatch, vc_export_state, ...) behind a
 * lock, or confine the manager to one thread. Distinct managers are fully
 * independent and may be used concurrently. Host callbacks (log sink, effect
 * handler) are invoked synchronously on the calling thread, inside the dispatch.
 * Stateless functions (vc_tag_match, vc_alloc_str, vc_free_str, vc_version) are
 * safe from any thread.
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

/* ── Undo control (SPEC §6) ────────────────────────────────────────────────
 *
 * Undo is ON by default with a 200-frame bound, and every mutating command
 * snapshots the whole undoable slice (`mantles` + `active`) before it runs.
 * That memento is cheap for a DOCUMENT and expensive for a WORLD, and which one
 * lives in `mantles` is a decision only the host has made:
 *
 *   - a host whose runes are a design — a topology, a census, an archive — pays
 *     a copy proportional to a document, and should leave undo on;
 *   - a host whose runes are live instances (Void Unity's rule: a thing is a
 *     rune iff it can be the endpoint of a link, so every door, crate and NPC in
 *     a streaming map is one) pays a copy of the world per command. Measured on
 *     that host, 2026-08-28: one `set` at 4 000 runes cost 27.6 ms — longer than
 *     a 60 Hz frame — and building the world was quadratic, because each
 *     `rune new` deep-copies every rune already present.
 *
 * So the switch is the host's, exactly as vc_set_journal's is. The two together
 * are what make a single process able to hold an AUTHORING manager (undo on,
 * journal on, attributed, replayable) and a WORLD manager (both off) side by
 * side. Turning undo off does not change what any command does; it only means
 * `undo`/`redo` will fail, and they say so rather than reporting an empty stack.
 * `batch` stays atomic either way — it rolls back from its own saved copy.
 *
 * This is a switch, not a fix: the memento is still O(slice) when it is taken.
 * The fix is reified commands (okf/design/command-architecture.md), which two
 * hosts have now asked for. Thread rule is the manager's (SPEC §6). */

/* Turn undo/redo recording on (nonzero) or off (0). Safe at any point. Turning
 * it OFF also drops the frames already recorded — unlike vc_set_journal, which
 * keeps its entries, because an undo frame is only ever consumed by `undo` and
 * keeping unreachable frames would hold exactly the memory the host declined. */
VC_API void vc_set_undo(VC_Manager *m, int enabled);

/* Bound the undo (and redo) stack to `depth` frames; the oldest is dropped past
 * it. Default 200 (the SPEC §6 reference bound). Values below 1 clamp to 1;
 * lowering the depth trims the stacks immediately rather than at the next
 * mutation. Independent of vc_set_undo: depth is what a frame-keeping host pays
 * at rest, the switch is what it pays per command. */
VC_API void vc_set_undo_depth(VC_Manager *m, int depth);

/* ── The command journal (SPEC §6.2) ───────────────────────────────────────
 *
 * Undo is memento-based: a before-image, which is the right structure for
 * taking a change back on one device and the wrong one for describing a change
 * to anything else. The journal is the other half — each successful mutating
 * command REIFIED as data:
 *
 *   { "seq": 1, "command": "rune new dialogue title", "verb": "rune",
 *     "who": "ada"|null, "pure": true, "slice": "undo"|"view",
 *     "minted": ["rune_9f2c…"] }
 *
 * Three fields carry the weight:
 *  - `command` is the CANONICAL line (POSIX aliases already desugared), so the
 *    same change never records under two spellings.
 *  - `pure` is false iff the command could reach the host effect handler — the
 *    holiday boundary. An effectful command is not replayable, not invertible
 *    and not addressable by its result, so a consumer building a replayable or
 *    transmissible history MUST record only pure entries.
 *  - `minted` is the ids that exist after the command and did not before. Core
 *    mints ids from the PRNG, so replaying `command` alone produces DIFFERENT
 *    state; replaying the entry does not. (For an `undo`/`redo` entry these ids
 *    are restored rather than freshly minted — the verb says which.)
 *
 * OFF by default: a host that does not ask for the record pays neither the
 * entries nor the id-diff that fills `minted`. Journaling does not change what
 * any command does, so turning it on is safe at any point.
 *
 * Thread rule is the manager's (SPEC §6): serialize per instance. */

/* Turn the journal on (nonzero) or off (0). Turning it off keeps the entries
 * already recorded; use vc_journal_clear to drop them. */
VC_API void vc_set_journal(VC_Manager *m, int enabled);

/* The journal as a JSON array, oldest first. Caller frees with vc_free_str.
 * Returns NULL only if m is NULL. */
VC_API char *vc_export_journal(VC_Manager *m);

/* Drop every recorded entry. `seq` keeps counting — a sequence number is a name,
 * and a reused name would let two different commands be the same entry. */
VC_API void vc_journal_clear(VC_Manager *m);

/* Evaluate a SPEC §5 tag/filter expression against a bag of tags, without a
 * manager. `tags_json` is a JSON array of tag strings, e.g.
 *   ["month:june","type:event","alpha"]
 * Returns 1 (match), 0 (no match), or -1 on malformed input (NULL args or
 * `tags_json` not a JSON array of strings). An empty expression matches (1).
 * Name-as-tag: include the entity's name in the array to get SPEC §5 name
 * matching; likewise include "glyph:<g>" if glyph matching is wanted. This is
 * the ONE implementation of the filter grammar — hosts filtering holiday/
 * external entities should call this instead of reimplementing the grammar.
 * Stateless and thread-safe. */
VC_API int vc_tag_match(const char *expr, const char *tags_json);

/* ── The SPEC §6.1 command codec (stateless; no manager needed) ─────────────
 *
 * §6.1 is a rule every host must implement twice — once to encode a value into a
 * command, once to decode a command it is about to run — and four independent
 * codebases have now implemented it wrong, this one included. So it ships as
 * code rather than only as prose. All three are stateless and thread-safe.
 *
 * THE LAW, which conformance case 13 and `quote_arg`'s property test pin:
 *
 *     vc_argv_split_json(vc_arg_quote(v)).argv == [v]
 *
 * for every NUL-free byte string v — newlines, quotes, backslashes, control
 * characters and all. (NUL-free because this whole boundary is C strings; a
 * value containing a NUL cannot reach vc_dispatch at all, so the codec does not
 * pretend otherwise with a length parameter.) */

/* Quote an arbitrary value as ONE dispatcher argument. Caller frees. */
VC_API char *vc_arg_quote(const char *value);

/* Tokenize one command line exactly as vc_dispatch will.
 *   {"ok":true,"argv":["set","v","bio","two words"]}
 *   {"ok":false,"error":"unterminated quote (§6.1 rule 5)","argv":null}
 * Caller frees. */
VC_API char *vc_argv_split_json(const char *line);

/* Split a whole transcript into the statements it will run — the DECODER half,
 * for a host that must review a proposed transcript before dispatching it.
 * Boundaries are newline and `;` OUTSIDE quoted runs, so a newline inside a
 * value is data, not a new command; `#` comments are dropped.
 *   {"ok":true,"flat":true,"commands":[{"line":1,"text":"...","argv":[...]}]}
 *   {"ok":false,"error":"unterminated quote ...","line":7}
 * `flat` is false if any statement opens a block or begins with a SPEC §8
 * control word — a flat transcript is one whose effect can be read off its
 * statements without simulating it, which is what a submission gate wants to
 * know. Caller frees. */
VC_API char *vc_transcript_split_json(const char *src);

/* Library version (static string, do not free). */
VC_API const char *vc_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VOIDCORE_H */
