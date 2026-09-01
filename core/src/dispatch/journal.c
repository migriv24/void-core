/* journal.c — the command journal: reified commands (SPEC §6.2).
 *
 * Undo stays memento-based (undo.c) because a before-image is the simplest
 * *correct* way to take a change back on one device. The journal answers the
 * other question — *what happened, as data* — which a snapshot cannot: a
 * before-image is not addressable, not replayable, and not transmissible.
 *
 * The two are deliberately separate structures rather than one:
 *   - the undo stack is BOUNDED (200) and drops its oldest frame; a record that
 *     silently forgets is not a history.
 *   - undo/redo CONSUME frames, moving them between stacks; a record must gain
 *     an entry when a change is taken back, not lose one.
 *
 * Off by default (vc_set_journal), so a host that does not want the record pays
 * nothing — not the entries, and not the id-diff that fills `minted`.
 */
#include "vc_internal.h"
#include <stdlib.h>
#include <string.h>

/* ── minted ids: the id-set diff ──────────────────────────────────────────
 *
 * Core mints rune/mantle/binding ids from the OS PRNG, so replaying a command
 * string produces DIFFERENT state — which makes the string alone useless as a
 * record. The journal therefore stores the identity that was actually minted,
 * and replay becomes a function of the entry rather than of the text.
 *
 * It is computed by diffing the state's id set across the command rather than
 * by instrumenting vc_mint_id, for two reasons: the minting sites live in the
 * model layer, which has no manager to report to, and a recorder reached
 * through a global would break the ABI's promise that distinct managers are
 * independent across threads. The diff needs neither.
 */

/* Collect every `id` string in the document, skipping two kinds of subtree:
 *
 *  - `content`, because it is opaque by contract (SPEC §3.2) — an app-defined
 *    field that happens to be called "id" is none of the core's business.
 *  - `_baseline`, because it is a SNAPSHOT of `mantles`, not live content. An id
 *    inside it is a record that an identity existed, not an identity that
 *    exists. Counting it made `save` — which copies `mantles` into `_baseline`
 *    (SPEC §7 dirty-tracking) — report every id in the document as freshly
 *    minted, since the diff saw a second occurrence of each.
 *
 * Both exclusions are about the same thing: only count identities that are
 * really there, once. */
static void id_walk(cJSON *node, char ***out, int *n, int *cap) {
  if (!node) return;
  if (cJSON_IsObject(node)) {
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, node) {
      const char *k = it->string;
      if (k && (!strcmp(k, "content") || !strcmp(k, "_baseline")))
        continue; /* opaque, or a snapshot — do not count either */
      if (k && !strcmp(k, "id") && cJSON_IsString(it)) {
        if (*n >= *cap) {
          *cap = *cap ? *cap * 2 : 32;
          *out = (char **)realloc(*out, (size_t)*cap * sizeof(char *));
        }
        (*out)[(*n)++] = vc_strdup(it->valuestring);
        continue;
      }
      id_walk(it, out, n, cap);
    }
  } else if (cJSON_IsArray(node)) {
    cJSON *it = NULL;
    cJSON_ArrayForEach(it, node) id_walk(it, out, n, cap);
  }
}

static int id_cmp(const void *a, const void *b) {
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}

vc_id_set vc_id_snapshot(cJSON *state) {
  vc_id_set s;
  s.ids = NULL;
  s.count = 0;
  int cap = 0;
  id_walk(state, &s.ids, &s.count, &cap);
  if (s.count > 1) qsort(s.ids, (size_t)s.count, sizeof(char *), id_cmp);
  return s;
}

void vc_id_set_free(vc_id_set *s) {
  if (!s) return;
  for (int i = 0; i < s->count; i++) free(s->ids[i]);
  free(s->ids);
  s->ids = NULL;
  s->count = 0;
}

/* Sorted merge-diff: every id in `post` that is not in `pre`. O(n log n) via the
 * sort, which is what keeps this affordable on a large document. */
cJSON *vc_id_set_minted(const vc_id_set *pre, const vc_id_set *post) {
  cJSON *arr = cJSON_CreateArray();
  int i = 0, j = 0;
  while (j < post->count) {
    while (i < pre->count && strcmp(pre->ids[i], post->ids[j]) < 0) i++;
    if (i < pre->count && !strcmp(pre->ids[i], post->ids[j])) {
      i++; /* present before: not minted here */
    } else {
      cJSON_AddItemToArray(arr, cJSON_CreateString(post->ids[j]));
    }
    j++;
  }
  return arr;
}

/* ── the journal itself ───────────────────────────────────────────────────── */

void vc_journal_append(VC_Manager *m, const char *command, const char *verb,
                       int pure, const char *slice, cJSON *minted) {
  if (!m->journal_on) {
    if (minted) cJSON_Delete(minted);
    return;
  }
  if (m->journal_count >= m->journal_cap) {
    m->journal_cap = m->journal_cap ? m->journal_cap * 2 : 32;
    m->journal = (vc_journal_entry *)realloc(
        m->journal, (size_t)m->journal_cap * sizeof(vc_journal_entry));
  }
  vc_journal_entry *e = &m->journal[m->journal_count++];
  e->seq = ++m->journal_seq;
  e->command = vc_strdup(command);
  e->verb = vc_strdup(verb);
  const char *who = vc_actor(m->state);
  e->who = who ? vc_strdup(who) : NULL;
  e->pure = pure;
  e->slice = vc_strdup(slice);
  e->minted = minted ? minted : cJSON_CreateArray();
}

cJSON *vc_journal_json(VC_Manager *m) {
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < m->journal_count; i++) {
    vc_journal_entry *e = &m->journal[i];
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "seq", (double)e->seq);
    cJSON_AddStringToObject(o, "command", e->command);
    cJSON_AddStringToObject(o, "verb", e->verb);
    if (e->who) cJSON_AddStringToObject(o, "who", e->who);
    else cJSON_AddItemToObject(o, "who", cJSON_CreateNull());
    cJSON_AddBoolToObject(o, "pure", e->pure);
    cJSON_AddStringToObject(o, "slice", e->slice);
    cJSON_AddItemToObject(o, "minted", cJSON_Duplicate(e->minted, 1));
    cJSON_AddItemToArray(arr, o);
  }
  return arr;
}

void vc_journal_clear_all(VC_Manager *m) {
  for (int i = 0; i < m->journal_count; i++) {
    vc_journal_entry *e = &m->journal[i];
    free(e->command);
    free(e->verb);
    free(e->who);
    free(e->slice);
    if (e->minted) cJSON_Delete(e->minted);
  }
  free(m->journal);
  m->journal = NULL;
  m->journal_count = m->journal_cap = 0;
  /* `seq` is NOT reset: a sequence number that gets reused would let two
   * different commands share a name, which is the one thing a record may not do. */
}
