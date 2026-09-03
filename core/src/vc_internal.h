/*
 * vc_internal.h — shared internal surface for the Void Core modules.
 * NOT part of the public ABI. Modules include this; consumers include voidcore.h.
 *
 * Design note (SPEC §2/§3, §11): the canonical in-memory representation IS the
 * serializable state document, held as a cJSON tree. This makes round-tripping
 * the state document free and keeps `content` genuinely opaque to the core.
 */
#ifndef VC_INTERNAL_H
#define VC_INTERNAL_H

#include <stddef.h>
#include "cJSON.h"
#include "voidcore.h"

/* The ONE version string — vc_version() and the `version` verb both return it.
 * (Bump here; 0.2.2/0.2.3 drifted because it was duplicated in vc_manager.c.) */
#define VC_VERSION_STR "0.2.13"

/* An undo frame: a snapshot of the undoable slice of state (mantles + active),
 * labelled by the command that produced it (SPEC §6). v0 is memento-based; the
 * deeper "reified command" idea is parked in notes/command-architecture.md. */
typedef struct {
  char *label;
  char *who;      /* config.actor at capture time, or NULL (SPEC §9 attribution) */
  cJSON *mantles; /* duplicated snapshot */
  cJSON *active;  /* duplicated snapshot */
} vc_undo_frame;

/* A journal entry: one mutating command, reified (SPEC §6.2). Where an undo
 * frame is a before-image, this is the change itself — addressable, replayable
 * and transmissible. See journal.c for why the two are separate structures. */
typedef struct {
  long seq;      /* 1-based, never reused within a manager */
  char *command; /* the CANONICAL argv-joined line (aliases already desugared) */
  char *verb;    /* canonical verb, for filtering without re-splitting */
  char *who;     /* config.actor at the time, or NULL (SPEC §9) */
  int pure;      /* 1 = model-only; 0 = crossed the holiday boundary (§6.2) */
  char *slice;   /* "undo" (mantles+active) or "view" (placement) */
  cJSON *minted; /* array of ids minted by this command (never NULL) */
} vc_journal_entry;

/* A sorted set of the `id` strings in a state document — the pre/post images
 * whose difference is a command's minted ids. */
typedef struct {
  char **ids;
  int count;
} vc_id_set;

/* The manager owns exactly one state document (SPEC §2) + undo/redo stacks. */
struct VC_Manager {
  cJSON *state;
  cJSON *glyphs; /* glyph registry (host config, NOT in the state document) */
  cJSON *log;    /* in-memory log ring (SPEC §9) */
  VC_LogFn log_sink;
  void *log_user;
  VC_EffectFn effect; /* host effect handler (save/deploy/build/preview) */
  void *effect_user;
  int suppress_undo; /* set during batch so sub-commands don't push frames */
  /* undo/redo (SPEC §6) — ON by default, but host-controllable like the journal.
   * The memento is a copy of the whole undoable slice, so a host whose `mantles`
   * hold a WORLD rather than a document pays that copy on every mutation. */
  int undo_on;
  int undo_depth; /* max frames kept; the oldest is dropped past it */
  vc_undo_frame *undo;
  int undo_count, undo_cap;
  vc_undo_frame *redo;
  int redo_count, redo_cap;
  /* the command journal (SPEC §6.2) — opt-in, so it costs nothing when unused */
  int journal_on;
  long journal_seq;
  vc_journal_entry *journal;
  int journal_count, journal_cap;
  /* Observed holiday crossing: set whenever the effect handler is actually
   * invoked, cleared at the start of each top-level dispatch. It can only
   * UPGRADE a command's classification pure -> effectful, never downgrade one
   * (see is_effectful() in dispatch.c). */
  int effect_fired;
};

/* ── state document (model/store, currently in vc_manager.c) ──────────────── */
cJSON *vc_state_new(void);              /* the empty state document (SPEC §2) */
cJSON *vc_active_mantle(cJSON *state);  /* resolve state.active.mantle, or NULL */
/* Session actor for attribution (SPEC §9): config.actor if a non-empty string,
 * else NULL. Set/cleared through the `config` verb — session-scoped by design. */
const char *vc_actor(cJSON *state);

/* ── ids / strings (util) ────────────────────────────────────────────────── */
/* Mint "<prefix>_<hex>" into out. v0 PRNG; harden to a CSPRNG later. */
void vc_mint_id(const char *prefix, char *out, size_t out_sz);
char *vc_strdup(const char *s); /* malloc'd copy; NULL-safe (NULL -> "") */

/* ── logging (SPEC §9) ───────────────────────────────────────────────────── */
void vc_log(struct VC_Manager *m, const char *level, const char *op,
            const char *fmt, ...);
cJSON *vc_log_buffer(struct VC_Manager *m);

/* ── lifecycle / dirty-tracking (SPEC §7) ────────────────────────────────── */
void vc_snapshot_baseline(cJSON *state); /* _baseline := copy of mantles */
cJSON *vc_compute_diff(cJSON *state);    /* {added,removed,changed} by mantle/rune */
int vc_is_dirty(cJSON *state);

/* ── undo/redo (SPEC §6) ─────────────────────────────────────────────────── */
/* Snapshot the current undoable slice, labelled by `command`. The caller either
 * commits the frame (on a successful mutation) or frees it (on failure). */
vc_undo_frame vc_undo_capture(struct VC_Manager *m, const char *command);
void vc_undo_trim(struct VC_Manager *m); /* enforce m->undo_depth on both stacks */
int vc_undo_default_depth(void);         /* the SPEC §6 reference bound (200) */
void vc_undo_commit(struct VC_Manager *m, vc_undo_frame *snap); /* push + clear redo */
void vc_undo_frame_free(vc_undo_frame *f);
int vc_undo(struct VC_Manager *m, int n); /* returns count actually undone */
int vc_redo(struct VC_Manager *m, int n);
cJSON *vc_history(struct VC_Manager *m);  /* array of undo labels, oldest first */
void vc_undo_clear(struct VC_Manager *m); /* free both stacks (used by destroy) */

/* ── command journal (SPEC §6.2) ─────────────────────────────────────────── */
/* Takes ownership of `minted` (and frees it if journaling is off). */
void vc_journal_append(struct VC_Manager *m, const char *command,
                       const char *verb, int pure, const char *slice,
                       cJSON *minted);
cJSON *vc_journal_json(struct VC_Manager *m);  /* the record as an array */
void vc_journal_clear_all(struct VC_Manager *m);
vc_id_set vc_id_snapshot(cJSON *state);
void vc_id_set_free(vc_id_set *s);
cJSON *vc_id_set_minted(const vc_id_set *pre, const vc_id_set *post);

/* ── spirit (model) — SPEC §3.1 ──────────────────────────────────────────── */
cJSON *vc_spirit_new(const char *prefix, const char *name);

/* ── rune (model) — SPEC §3.2 ────────────────────────────────────────────── */
cJSON *vc_rune_new(const char *glyph, const char *name);
const char *vc_rune_name(const cJSON *rune);
const char *vc_rune_id(const cJSON *rune);
int vc_rune_matches_ref(const cJSON *rune, const char *ref); /* by name OR id */

/* ── tags (SPEC §5) ──────────────────────────────────────────────────────── */
/* membership: a rune matches a TAG via its tags[], its name, or glyph:<name> */
int vc_rune_has_tag(const cJSON *rune, const char *tag);
/* evaluate the filter-expression grammar against a rune; empty expr => true */
int vc_filter_eval(const cJSON *rune, const char *expr);
/* classify a tag into a fundamental axis (where/what/who/when/state/free) */
const char *vc_axis_of(const char *tag);

/* ── glyph registry (model) — SPEC §3.3 ──────────────────────────────────── */
cJSON *vc_glyphs_new_builtin(void);              /* registry with the 7 built-ins */
cJSON *vc_glyph_find(cJSON *glyphs, const char *name);          /* def or NULL */
int vc_glyph_register(cJSON *glyphs, const char *glyph_json);   /* 1 on success */
cJSON *vc_glyph_default_content(const cJSON *glyphdef);         /* {field:""...} */

/* ── mantle (model) — SPEC §3.4 ──────────────────────────────────────────── */
cJSON *vc_mantle_new(const char *name, const char *domain);
const char *vc_mantle_name(const cJSON *mantle);
cJSON *vc_mantle_runes(cJSON *mantle);
cJSON *vc_mantle_find_rune(cJSON *mantle, const char *ref);
int vc_mantle_add_rune(cJSON *mantle, cJSON *rune);    /* 0 if duplicate name */
int vc_mantle_remove_rune(cJSON *mantle, const char *ref);
/* Add or update a layout edge (a "link") from->to: relation label, weight, and
 * direction. Updates weight/directed if the (from,to,relation) edge already exists.
 * Links may dangle (endpoints need not exist). SPEC §3.4. */
cJSON *vc_mantle_add_edge(cJSON *mantle, const char *from, const char *to,
                          const char *relation, double weight, int directed);
/* Remove every edge matching from->to (and relation, if non-NULL/non-empty).
 * Returns the count removed. */
int vc_mantle_remove_edge(cJSON *mantle, const char *from, const char *to,
                          const char *relation);

/* ── bindings (model) — SPEC §3.6 ────────────────────────────────────────── */
cJSON *vc_bindings(cJSON *state); /* ensure + return state.bindings */
int vc_parse_ref(cJSON *state, const char *ref, const char *def_mantle,
                 cJSON **mantle_out, cJSON **rune_out);
cJSON *vc_binding_new(const char *name, cJSON *from, cJSON *to, const char *note);

/* ── dispatch ────────────────────────────────────────────────────────────── */
/* Route one command; returns a {ok,lines,data} cJSON object (caller deletes). */
cJSON *vc_dispatch_json(struct VC_Manager *m, const char *command);

/* ── Voidscript (SPEC §8) ────────────────────────────────────────────────── */
/* Run a script source against the dispatcher; returns {ok,lines,data}.
 * `args` (or NULL) supplies $1.. and $@. */
cJSON *vc_script_run(struct VC_Manager *m, const char *source, cJSON *args);

/* ── args (dispatch) — SPEC §6 argument parsing ──────────────────────────── */
typedef struct {
  char **items;
  int count;
  /* SPEC §6.1 rule 5: a quoted run that is still open at end of input. Since
   * 0.2.7 that is an ERROR rather than a silent run-to-end — it was the single
   * property that made every bug in this class quiet. */
  int unterminated;
} vc_argv;
/* The ONE §6.1 quote automaton. Every scanner in this tree that needs to know
 * whether it is inside a quoted run calls this — the argv tokenizer and the
 * Voidscript statement reader. Do not write a second one. */
int vc_quote_step(const char *p, char *quote, int *emit);
char *vc_arg_quote(const char *value);  /* the §6.1 encoder; caller frees */
vc_argv vc_argv_split(const char *line); /* the §6.1 decoder (quote-aware) */
char *vc_argv_join(const vc_argv *a);   /* canonical re-splittable line; frees */
/* dispatch/codec.c — the exported §6.1 codec (see include/voidcore.h) */
char *vc_argv_split_json(const char *line);
char *vc_transcript_split_json(const char *src);
/* Rewrite POSIX-flavored aliases to their canonical verb form (SPEC §7) —
 * argument-aware (e.g. `rm x` -> `rune rm x`), applied before routing. */
void vc_argv_desugar(vc_argv *a);
void vc_argv_free(vc_argv *a);

#endif /* VC_INTERNAL_H */
