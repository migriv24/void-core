/* undo.c — undo/redo via state snapshots (SPEC §6 mutation invariants).
 *
 * v0 is memento-based: each mutating command snapshots the undoable slice
 * (mantles + active) before it runs; on success the snapshot is committed to the
 * undo stack and the redo stack is cleared. This is the simplest *correct*
 * implementation and matches the SPEC's "snapshot of mantles+active" wording.
 *
 * The user wants to explore a richer "commander-based architecture" (reified
 * command objects with explicit inverses) inside the core — see
 * the OKF design page okf/design/command-architecture.md. That is intentionally NOT done here yet.
 */
#include "vc_internal.h"
#include <stdlib.h>
#include <string.h>

#define VC_UNDO_MAX 200 /* bounded stack (SPEC §6 reference impl) */

static cJSON *dup_field(cJSON *state, const char *key) {
  return cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(state, key), 1);
}

vc_undo_frame vc_undo_capture(VC_Manager *m, const char *command) {
  vc_undo_frame f;
  f.label = vc_strdup(command);
  const char *who = vc_actor(m->state); /* attribution (SPEC §9) */
  f.who = who ? vc_strdup(who) : NULL;
  f.mantles = dup_field(m->state, "mantles");
  f.active = dup_field(m->state, "active");
  return f;
}

void vc_undo_frame_free(vc_undo_frame *f) {
  if (!f) return;
  free(f->label);
  free(f->who);
  if (f->mantles) cJSON_Delete(f->mantles);
  if (f->active) cJSON_Delete(f->active);
  f->label = NULL;
  f->who = NULL;
  f->mantles = NULL;
  f->active = NULL;
}

static void stack_push(vc_undo_frame **arr, int *count, int *cap, vc_undo_frame f) {
  if (*count >= *cap) {
    *cap = *cap ? *cap * 2 : 16;
    *arr = (vc_undo_frame *)realloc(*arr, (size_t)*cap * sizeof(vc_undo_frame));
  }
  (*arr)[(*count)++] = f;
}

void vc_undo_commit(VC_Manager *m, vc_undo_frame *snap) {
  /* a new mutation invalidates the redo history */
  for (int i = 0; i < m->redo_count; i++) vc_undo_frame_free(&m->redo[i]);
  m->redo_count = 0;

  stack_push(&m->undo, &m->undo_count, &m->undo_cap, *snap);

  if (m->undo_count > VC_UNDO_MAX) { /* drop the oldest frame */
    vc_undo_frame_free(&m->undo[0]);
    memmove(&m->undo[0], &m->undo[1],
            (size_t)(m->undo_count - 1) * sizeof(vc_undo_frame));
    m->undo_count--;
  }
}

/* The view slice (SPEC §3.2/§6): `placement` is OUTSIDE the undo slice. Before a
 * snapshot is restored, carry each surviving rune's CURRENT placement into the
 * incoming mantles (matched by mantle name + rune name), so undo/redo never
 * moves what the user placed. A rune only in the snapshot (recreated by the
 * restore) keeps its snapshot placement — better than losing it. */
static void placement_overlay(cJSON *incoming, cJSON *current) {
  cJSON *im = NULL;
  cJSON_ArrayForEach(im, incoming) {
    cJSON *cm = NULL, *cur_mantle = NULL;
    cJSON_ArrayForEach(cm, current) {
      if (!strcmp(vc_mantle_name(cm), vc_mantle_name(im))) { cur_mantle = cm; break; }
    }
    if (!cur_mantle) continue;
    cJSON *ir = NULL;
    cJSON_ArrayForEach(ir, cJSON_GetObjectItemCaseSensitive(im, "runes")) {
      cJSON *cr = NULL, *cur_rune = NULL;
      cJSON_ArrayForEach(cr, cJSON_GetObjectItemCaseSensitive(cur_mantle, "runes")) {
        if (!strcmp(vc_rune_name(cr), vc_rune_name(ir))) { cur_rune = cr; break; }
      }
      if (!cur_rune) continue;
      cJSON *p = cJSON_GetObjectItemCaseSensitive(cur_rune, "placement");
      cJSON *dup = p ? cJSON_Duplicate(p, 1) : cJSON_CreateNull();
      if (!cJSON_ReplaceItemInObjectCaseSensitive(ir, "placement", dup))
        cJSON_AddItemToObject(ir, "placement", dup);
    }
  }
}

/* Restore `f`'s snapshot into state, after stashing the current state onto
 * `other` so the move is reversible. Consumes f's cJSON. */
static void apply_frame(VC_Manager *m, vc_undo_frame *f, vc_undo_frame **other,
                        int *other_count, int *other_cap) {
  vc_undo_frame cur = vc_undo_capture(m, f->label);
  stack_push(other, other_count, other_cap, cur);
  placement_overlay(f->mantles,
                    cJSON_GetObjectItemCaseSensitive(m->state, "mantles"));
  cJSON_ReplaceItemInObjectCaseSensitive(m->state, "mantles", f->mantles);
  cJSON_ReplaceItemInObjectCaseSensitive(m->state, "active", f->active);
  f->mantles = NULL; /* ownership moved into state */
  f->active = NULL;
  free(f->label);
  f->label = NULL;
  free(f->who);
  f->who = NULL;
}

int vc_undo(VC_Manager *m, int n) {
  int done = 0;
  while (n > 0 && m->undo_count > 0) {
    vc_undo_frame f = m->undo[--m->undo_count];
    apply_frame(m, &f, &m->redo, &m->redo_count, &m->redo_cap);
    done++;
    n--;
  }
  return done;
}

int vc_redo(VC_Manager *m, int n) {
  int done = 0;
  while (n > 0 && m->redo_count > 0) {
    vc_undo_frame f = m->redo[--m->redo_count];
    apply_frame(m, &f, &m->undo, &m->undo_count, &m->undo_cap);
    done++;
    n--;
  }
  return done;
}

cJSON *vc_history(VC_Manager *m) {
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < m->undo_count; i++)
    cJSON_AddItemToArray(arr, cJSON_CreateString(m->undo[i].label));
  return arr;
}

void vc_undo_clear(VC_Manager *m) {
  for (int i = 0; i < m->undo_count; i++) vc_undo_frame_free(&m->undo[i]);
  for (int i = 0; i < m->redo_count; i++) vc_undo_frame_free(&m->redo[i]);
  free(m->undo);
  free(m->redo);
  m->undo = m->redo = NULL;
  m->undo_count = m->redo_count = m->undo_cap = m->redo_cap = 0;
}
