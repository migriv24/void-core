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
  int have_snap = 0;
  vc_undo_frame snap;
  if (is_mutating(&a) && !m->suppress_undo) {
    snap = vc_undo_capture(m, command);
    have_snap = 1;
  }

  res = vc_verbs_query(m, state, a, v);
  if (!res) res = vc_verbs_edit(m, state, a, v);
  if (!res) res = vc_verbs_graph(m, state, a, v);
  if (!res) res = vc_verbs_lifecycle(m, state, a, v);
  if (!res) res = vc_verbs_script(m, state, a, v);
  if (!res) res = res_fail("unknown verb: %s (try 'help')", v);


  /* commit or discard the pre-mutation snapshot based on the outcome */
  if (have_snap) {
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(res, "ok"))) {
      vc_undo_commit(m, &snap);
      /* the mutation spine (SPEC §9): every successful top-level mutating
       * command is logged (with `who` when config.actor is set), so a shared
       * seam — human CLI, GUI gestures, agents — is fully auditable. batch
       * sub-commands run under suppress_undo and are covered by their batch. */
      vc_log(m, "INFO", v, "%s", command);
    } else {
      vc_undo_frame_free(&snap);
    }
  } else if (is_view_mutation(&a) && !m->suppress_undo &&
             cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(res, "ok"))) {
    /* view slice: spine-logged like any mutation, but no undo frame */
    vc_log(m, "INFO", v, "%s", command);
  }

  vc_argv_free(&a);
  return res;
}

