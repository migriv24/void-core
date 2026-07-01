/* lifecycle.c — dirty-tracking against _baseline (SPEC §7).
 *
 * _baseline holds a snapshot of `mantles` taken at the last `save`. status/diff
 * compare the working mantles to it; revert restores from it; save replaces it.
 * Comparison is rune-level, keyed by "mantle/rune-name", using serialized JSON
 * equality (cheap and exact for v0).
 */
#include "vc_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vc_snapshot_baseline(cJSON *state) {
  cJSON *mantles = cJSON_GetObjectItemCaseSensitive(state, "mantles");
  cJSON_ReplaceItemInObjectCaseSensitive(state, "_baseline",
                                         cJSON_Duplicate(mantles, 1));
}

/* find a mantle by name within an array of mantles */
static cJSON *mantle_by_name(cJSON *mantles, const char *name) {
  cJSON *m = NULL;
  cJSON_ArrayForEach(m, mantles) {
    if (!strcmp(vc_mantle_name(m), name)) return m;
  }
  return NULL;
}

/* serialized equality of two runes */
static int rune_equal(cJSON *a, cJSON *b) {
  char *sa = cJSON_PrintUnformatted(a);
  char *sb = cJSON_PrintUnformatted(b);
  int eq = sa && sb && !strcmp(sa, sb);
  free(sa);
  free(sb);
  return eq;
}

cJSON *vc_compute_diff(cJSON *state) {
  cJSON *diff = cJSON_CreateObject();
  cJSON *added = cJSON_AddArrayToObject(diff, "added");
  cJSON *removed = cJSON_AddArrayToObject(diff, "removed");
  cJSON *changed = cJSON_AddArrayToObject(diff, "changed");

  cJSON *cur = cJSON_GetObjectItemCaseSensitive(state, "mantles");
  cJSON *base = cJSON_GetObjectItemCaseSensitive(state, "_baseline");

  char key[256];

  /* added / changed: walk current, compare to baseline */
  cJSON *cm = NULL;
  cJSON_ArrayForEach(cm, cur) {
    const char *mname = vc_mantle_name(cm);
    cJSON *bm = mantle_by_name(base, mname);
    cJSON *r = NULL;
    cJSON_ArrayForEach(r, vc_mantle_runes(cm)) {
      const char *rname = vc_rune_name(r);
      snprintf(key, sizeof key, "%s/%s", mname, rname);
      cJSON *br = bm ? vc_mantle_find_rune(bm, rname) : NULL;
      if (!br)
        cJSON_AddItemToArray(added, cJSON_CreateString(key));
      else if (!rune_equal(r, br))
        cJSON_AddItemToArray(changed, cJSON_CreateString(key));
    }
  }

  /* removed: walk baseline, anything missing from current */
  cJSON *bm = NULL;
  cJSON_ArrayForEach(bm, base) {
    const char *mname = vc_mantle_name(bm);
    cJSON *cm2 = mantle_by_name(cur, mname);
    cJSON *r = NULL;
    cJSON_ArrayForEach(r, vc_mantle_runes(bm)) {
      const char *rname = vc_rune_name(r);
      if (!cm2 || !vc_mantle_find_rune(cm2, rname)) {
        snprintf(key, sizeof key, "%s/%s", mname, rname);
        cJSON_AddItemToArray(removed, cJSON_CreateString(key));
      }
    }
  }
  return diff;
}

int vc_is_dirty(cJSON *state) {
  cJSON *d = vc_compute_diff(state);
  int dirty = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(d, "added")) ||
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(d, "removed")) ||
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(d, "changed"));
  cJSON_Delete(d);
  return dirty;
}
