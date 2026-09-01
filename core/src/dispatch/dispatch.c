/* dispatch.c — the one command dispatcher (SPEC §6, §7): the thin router.
 * Splits the command, snapshots for undo on a mutating verb, then dispatches to a
 * verb-family module (verbs_query/edit/graph/lifecycle/script.c). Shared helpers
 * live in dispatch_util.c; argument tokenizing in args.c; undo in undo.c. */
#include "dispatch_internal.h"
#include <stdlib.h>
#include <string.h>

/* Which verbs mutate the undoable slice (mantles/active)? These snapshot before
 * running (SPEC §6). undo/redo/history are NOT here — they manage the stacks. */
static int is_mutating(const vc_argv *a) {
  const char *v = a->items[0];
  if (!strcmp(v, "set") || !strcmp(v, "setjson") || !strcmp(v, "facet") ||
      !strcmp(v, "tag"))
    return 1;
  if (!strcmp(v, "rune") && a->count >= 2) {
    const char *s = a->items[1];
    return !strcmp(s, "new") || !strcmp(s, "rm") || !strcmp(s, "rename") ||
           !strcmp(s, "dup") || !strcmp(s, "move");
  }
  if (!strcmp(v, "mantle") && a->count >= 2) {
    const char *s = a->items[1];
    return !strcmp(s, "new") || !strcmp(s, "rm") || !strcmp(s, "rename");
  }
  if (!strcmp(v, "revert") || !strcmp(v, "batch")) return 1;
  if (!strcmp(v, "link") || !strcmp(v, "unlink")) return 1;
  if (!strcmp(v, "relate") || !strcmp(v, "unrelate")) return 1;
  if (!strcmp(v, "rule") && a->count >= 2 &&
      (!strcmp(a->items[1], "add") || !strcmp(a->items[1], "rm") ||
       !strcmp(a->items[1], "clear")))
    return 1;
  return 0;
}

/* View-slice mutations (SPEC §3.2/§6/§7 `place`): on the mutation spine
 * (logged) but NOT undoable — no snapshot, no history frame. A bare
 * `place <ref>` (possibly with a `--flag` like the capture-appended --json)
 * is a query and stays off the spine; coordinates or --clear make it a write. */
static int is_view_mutation(const vc_argv *a) {
  if (strcmp(a->items[0], "place") || a->count < 3) return 0;
  for (int i = 2; i < a->count; i++) {
    if (!strcmp(a->items[i], "--clear")) return 1;
    if (a->items[i][0] == '-' && a->items[i][1] == '-') continue;
    return 1; /* a coordinate token */
  }
  return 0;
}

/* PURE vs EFFECTFUL (SPEC §6.2) — "probably the single most important
 * distinction to get right" (okf/design/command-architecture.md §2), and the
 * one that decides whether a command can be honestly recorded at all.
 *
 * A command is **effectful** iff it can reach the host through the effect
 * handler — the holiday boundary. Those five verbs are the complete list,
 * because vc_set_effect_handler is the only way out of the core (SPEC §9).
 * Everything else touches nothing but the state document and is **pure**:
 * replayable, invertible, and addressable by its result.
 *
 * The classification is STATIC — a function of the verb, not of what happened —
 * so two peers running the same command agree on it even when only one of them
 * has an effect handler registered. A host-dependent answer would let the same
 * command be a recordable change on one device and not on another, which is
 * exactly the divergence this distinction exists to prevent. */
static int is_effectful(const vc_argv *a) {
  const char *v = a->items[0];
  return !strcmp(v, "save") || !strcmp(v, "deploy") || !strcmp(v, "build") ||
         !strcmp(v, "preview") || !strcmp(v, "effect");
}

/* `undo`/`redo` change the undoable slice but must never take a snapshot of
 * themselves, so they are deliberately absent from is_mutating(). The JOURNAL
 * still has to see them: a record that omits taking a change back replays into
 * a state the author never had. */
static int is_rewind(const vc_argv *a) {
  const char *v = a->items[0];
  return !strcmp(v, "undo") || !strcmp(v, "redo");
}

/* ── the router ── */
cJSON *vc_dispatch_json(VC_Manager *m, const char *command) {
  vc_argv a = vc_argv_split(command);
  if (a.count == 0) {
    vc_argv_free(&a);
    return res_fail("empty command");
  }
  /* SPEC §6.1 rule 5: refuse a command whose last argument never closed its
   * quote. The argument is necessarily wrong (it has swallowed everything after
   * it), and failing loudly here is the difference between a host noticing its
   * quoting bug and storing truncated content with ok:true. */
  if (a.unterminated) {
    const char *v0 = a.items[0];
    cJSON *bad = res_fail("unterminated quote in argument %d of '%s' "
                          "(SPEC §6.1: quote the value with vc_arg_quote)",
                          a.count - 1, v0);
    vc_log(m, "ERROR", v0, "unterminated quote: %s", command);
    vc_argv_free(&a);
    return bad;
  }
  /* POSIX aliases desugar to canonical argv first (SPEC §7), so is_mutating and
   * the undo label see the same command every downstream consumer does. */
  vc_argv_desugar(&a);
  const char *v = a.items[0];
  cJSON *state = m->state;
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)err;

  /* Snapshot before a mutating verb; commit on success, discard on failure
   * (so a failed mutation neither pollutes history nor clears redo). */
  int mutates = is_mutating(&a);
  int view = !mutates && is_view_mutation(&a);
  int top = !m->suppress_undo; /* batch sub-commands are covered by their batch */
  int have_snap = 0;
  vc_undo_frame snap;
  /* `m->undo_on` is the host's switch (vc_set_undo). With it off no memento is
   * taken at all — which is the whole point, since the memento is a copy of the
   * entire undoable slice and that is what makes a world-sized `mantles`
   * quadratic to build. `batch` atomicity is unaffected: it rolls back from its
   * own saved copy (verbs_script.c), not from the undo stack. */
  if (mutates && top && m->undo_on) {
    snap = vc_undo_capture(m, command);
    have_snap = 1;
  }

  /* The journal's pre-image (SPEC §6.2). Only taken when the journal is on and
   * this command could record something, so an unjournaled host walks nothing.
   * `effect_fired` is cleared per top-level dispatch so a `batch` can observe
   * whether any sub-command crossed the holiday boundary. */
  int rewind = !mutates && !view && is_rewind(&a);
  /* An effectful verb records too, even when it touches no slice of the state
   * document (`deploy`, `preview`, a read-only `effect`). Leaving it out would
   * make `pure` a constant true and leave a consumer unable to tell "nothing
   * effectful happened" from "something effectful happened and was not
   * recorded" — and the second is what silently drops a deploy from a replay. */
  int effectful = is_effectful(&a);
  int journaling =
      m->journal_on && top && (mutates || view || rewind || effectful);
  vc_id_set pre;
  pre.ids = NULL;
  pre.count = 0;
  if (top) m->effect_fired = 0;
  if (journaling) pre = vc_id_snapshot(state);

  res = vc_verbs_query(m, state, a, v);
  if (!res) res = vc_verbs_edit(m, state, a, v);
  if (!res) res = vc_verbs_graph(m, state, a, v);
  if (!res) res = vc_verbs_lifecycle(m, state, a, v);
  if (!res) res = vc_verbs_script(m, state, a, v);
  if (!res) res = res_fail("unknown verb: %s (try 'help')", v);


  int ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(res, "ok"));

  /* commit or discard the pre-mutation snapshot based on the outcome */
  if (have_snap) {
    if (ok) {
      vc_undo_commit(m, &snap);
      /* the mutation spine (SPEC §9): every successful top-level mutating
       * command is logged (with `who` when config.actor is set), so a shared
       * seam — human CLI, GUI gestures, agents — is fully auditable. batch
       * sub-commands run under suppress_undo and are covered by their batch. */
      vc_log(m, "INFO", v, "%s", command);
    } else {
      vc_undo_frame_free(&snap);
    }
  } else if (view && top && ok) {
    /* view slice: spine-logged like any mutation, but no undo frame */
    vc_log(m, "INFO", v, "%s", command);
  }

  /* The journal (SPEC §6.2): the same set of commands the spine logs, recorded
   * as data rather than as a sentence. A failed command records nothing — it
   * changed nothing, and a record of attempts is a different artifact (the log
   * already is one). */
  if (journaling && ok) {
    vc_id_set post = vc_id_snapshot(state);
    /* The canonical line, not the raw one: `rm x` and `rune rm x` are the same
     * change and must not record as two different ones. */
    char *canon = vc_argv_join(&a);
    /* Static classification, upgraded (never downgraded) by observation: a
     * `batch` is pure only if nothing inside it reached the host. */
    int pure = !effectful && !m->effect_fired;
    /* Where the change landed: the undoable slice, the view slice, or nowhere in
     * the state document at all (it went out through the holiday boundary). */
    const char *slice = view ? "view" : (mutates || rewind) ? "undo" : "host";
    vc_journal_append(m, canon ? canon : command, v, pure, slice,
                      vc_id_set_minted(&pre, &post));
    free(canon);
    vc_id_set_free(&post);
  }
  if (journaling) vc_id_set_free(&pre);

  vc_argv_free(&a);
  return res;
}

