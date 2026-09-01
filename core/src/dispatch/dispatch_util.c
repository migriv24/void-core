/* dispatch_util.c — shared result builders + rune/tag helpers used by every
 * verb-family module (declared in dispatch_internal.h). Extracted from the
 * original monolithic dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *vc_facet_keys[6] = {"who", "what", "when", "where", "why", "how"};

/* Resolve a mantle by name in the state document, or NULL. Mantle names are
 * unique (`mantle new` / `mantle rename` reject a taken one), so this answers a
 * yes/no question exactly — which is what lets `validate` tell a link endpoint
 * that names a mantle apart from one that names nothing at all (SPEC §3.7). */
cJSON *vc_state_find_mantle(cJSON *state, const char *name) {
  if (!name) return NULL;
  cJSON *mm = NULL;
  cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles"))
    if (!strcmp(vc_mantle_name(mm), name)) return mm;
  return NULL;
}

/* ── result builders ─────────────────────────────────────────────────────── */
cJSON *res_make(int ok) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddBoolToObject(r, "ok", ok ? 1 : 0);
  cJSON_AddItemToObject(r, "lines", cJSON_CreateArray());
  cJSON_AddItemToObject(r, "data", cJSON_CreateNull());
  return r;
}

/* Format one result line with NO length cap.
 *
 * It used to be a 1024-byte stack buffer, which truncated any line carrying a
 * long value — `set` echoes the value it stored — and, because the cut landed
 * mid-sequence, produced INVALID UTF-8 inside the JSON result. The host's parse
 * then threw on a command that had actually succeeded: a ~500-character value in
 * any non-ASCII script was enough. The tokenizer has grown its token buffer
 * dynamically since day one for exactly this reason; the result path had not
 * caught up. */
static char *vformat_dup(const char *fmt, va_list ap) {
  va_list cp;
  va_copy(cp, ap);
  int need = vsnprintf(NULL, 0, fmt, cp);
  va_end(cp);
  if (need < 0) return NULL;
  char *buf = (char *)malloc((size_t)need + 1);
  if (!buf) return NULL;
  vsnprintf(buf, (size_t)need + 1, fmt, ap);
  return buf;
}

void res_line(cJSON *r, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char *buf = vformat_dup(fmt, ap);
  va_end(ap);
  cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(r, "lines"),
                       cJSON_CreateString(buf ? buf : ""));
  free(buf);
}

void res_set_data(cJSON *r, cJSON *data) {
  cJSON_ReplaceItemInObjectCaseSensitive(r, "data", data);
}

cJSON *res_fail(const char *fmt, ...) {
  cJSON *r = res_make(0);
  va_list ap;
  va_start(ap, fmt);
  char *buf = vformat_dup(fmt, ap);
  va_end(ap);
  cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(r, "lines"),
                       cJSON_CreateString(buf ? buf : ""));
  free(buf);
  return r;
}

const char *gstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
  return cJSON_IsString(v) ? v->valuestring : "";
}

/* require the active mantle; on failure fill *err with a ready result. */
cJSON *need_mantle(cJSON *state, cJSON **err) {
  cJSON *mt = vc_active_mantle(state);
  if (!mt) *err = res_fail("no active mantle (use 'mantle new <name>')");
  return mt;
}

/* Resolve a <ref> into a borrowed array of rune pointers (caller frees the array,
 * not the runes). "@<expr>" selects every rune matching the filter (SPEC §4/§5);
 * a plain name/id selects the single match (0 or 1). */
int collect_targets(cJSON *mt, const char *ref, cJSON ***out) {
  cJSON **arr = NULL;
  int n = 0, cap = 0;
  cJSON *runes = vc_mantle_runes(mt);
  cJSON *r = NULL;
  if (ref && ref[0] == '@') {
    const char *expr = ref + 1;
    cJSON_ArrayForEach(r, runes) {
      if (vc_filter_eval(r, expr)) {
        if (n >= cap) {
          cap = cap ? cap * 2 : 8;
          arr = (cJSON **)realloc(arr, (size_t)cap * sizeof(cJSON *));
        }
        arr[n++] = r;
      }
    }
  } else {
    r = vc_mantle_find_rune(mt, ref);
    if (r) {
      arr = (cJSON **)malloc(sizeof(cJSON *));
      arr[0] = r;
      n = 1;
    }
  }
  *out = arr;
  return n;
}

/* case-insensitive substring test (for `find`) */
int ci_contains(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t i = 0;
    for (; i < nl; i++) {
      char c = p[i];
      if (!c || tolower((unsigned char)c) != tolower((unsigned char)needle[i]))
        break;
    }
    if (i == nl) return 1;
  }
  return 0;
}

/* weighted tag graph (per-mantle): set tags[a].near[b] = w. The graph is stored
 * but not yet *reduced* — the rule engine that fires on adjacency is reserved
 * (okf/design/concept-brainstorm.md: "modeled as a net now, reduced later"). */
void set_near(cJSON *tags, const char *a, const char *b, double w) {
  cJSON *ta = cJSON_GetObjectItemCaseSensitive(tags, a);
  if (!ta) {
    ta = cJSON_CreateObject();
    cJSON_AddItemToObject(tags, a, ta);
  }
  cJSON *near = cJSON_GetObjectItemCaseSensitive(ta, "near");
  if (!near) {
    near = cJSON_CreateObject();
    cJSON_AddItemToObject(ta, "near", near);
  }
  cJSON_DeleteItemFromObjectCaseSensitive(near, b);
  cJSON_AddNumberToObject(near, b, w);
}

void del_near(cJSON *tags, const char *a, const char *b) {
  cJSON *ta = cJSON_GetObjectItemCaseSensitive(tags, a);
  if (!ta) return;
  cJSON *near = cJSON_GetObjectItemCaseSensitive(ta, "near");
  if (near) cJSON_DeleteItemFromObjectCaseSensitive(near, b);
}
