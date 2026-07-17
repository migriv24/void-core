/* vc_manager.c — the public C ABI (voidcore.h) + the state document.
 *
 * Every exported function is exception-free and NULL-tolerant: a bad handle or a
 * malformed command never crashes the host, it returns an error result. This is
 * the boundary discipline that keeps the C ABI safe to call from any language.
 */
#include "vc_internal.h"
#include <stdlib.h>
#include <string.h>

/* ── the empty state document (SPEC §2) ──────────────────────────────────── */
cJSON *vc_state_new(void) {
  cJSON *s = cJSON_CreateObject();
  cJSON_AddNumberToObject(s, "version", 1);
  cJSON_AddItemToObject(s, "domains", cJSON_CreateObject());
  cJSON_AddItemToObject(s, "mantles", cJSON_CreateArray());
  cJSON_AddItemToObject(s, "scripts", cJSON_CreateObject());
  cJSON_AddItemToObject(s, "config", cJSON_CreateObject());
  cJSON *active = cJSON_CreateObject();
  cJSON_AddItemToObject(active, "mantle", cJSON_CreateNull());
  cJSON_AddItemToObject(active, "domain", cJSON_CreateNull());
  cJSON_AddItemToObject(s, "active", active);
  cJSON_AddItemToObject(s, "bindings", cJSON_CreateArray());
  cJSON_AddItemToObject(s, "_baseline", cJSON_CreateArray());
  return s;
}

/* SPEC §2: a loaded document may be partial — hydrate any missing/mistyped
 * top-level container to its empty-state default so every verb can rely on the
 * containers existing (same discipline as rune hydration, §3.2). */
static void hydrate_container(cJSON *s, const char *key, int is_array) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(s, key);
  if (v && (is_array ? cJSON_IsArray(v) : cJSON_IsObject(v))) return;
  if (v) cJSON_DeleteItemFromObjectCaseSensitive(s, key);
  cJSON_AddItemToObject(s, key, is_array ? cJSON_CreateArray() : cJSON_CreateObject());
}

static void vc_state_hydrate(cJSON *s) {
  if (!cJSON_GetObjectItemCaseSensitive(s, "version"))
    cJSON_AddNumberToObject(s, "version", 1);
  hydrate_container(s, "domains", 0);
  hydrate_container(s, "mantles", 1);
  hydrate_container(s, "scripts", 0);
  hydrate_container(s, "config", 0);
  hydrate_container(s, "bindings", 1);
  hydrate_container(s, "_baseline", 1);
  hydrate_container(s, "active", 0);
  cJSON *active = cJSON_GetObjectItemCaseSensitive(s, "active");
  if (!cJSON_GetObjectItemCaseSensitive(active, "mantle"))
    cJSON_AddItemToObject(active, "mantle", cJSON_CreateNull());
  if (!cJSON_GetObjectItemCaseSensitive(active, "domain"))
    cJSON_AddItemToObject(active, "domain", cJSON_CreateNull());
}

const char *vc_actor(cJSON *state) {
  cJSON *cfg = cJSON_GetObjectItemCaseSensitive(state, "config");
  cJSON *actor = cfg ? cJSON_GetObjectItemCaseSensitive(cfg, "actor") : NULL;
  if (cJSON_IsString(actor) && actor->valuestring[0]) return actor->valuestring;
  return NULL;
}

cJSON *vc_active_mantle(cJSON *state) {
  cJSON *active = cJSON_GetObjectItemCaseSensitive(state, "active");
  cJSON *mname = active ? cJSON_GetObjectItemCaseSensitive(active, "mantle") : NULL;
  if (!cJSON_IsString(mname)) return NULL;
  cJSON *mm = NULL;
  cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles")) {
    if (!strcmp(vc_mantle_name(mm), mname->valuestring)) return mm;
  }
  return NULL;
}

/* ── public ABI ──────────────────────────────────────────────────────────── */
VC_Manager *vc_create(const char *state_json) {
  VC_Manager *m = (VC_Manager *)calloc(1, sizeof(VC_Manager));
  if (!m) return NULL;
  cJSON *parsed = state_json ? cJSON_Parse(state_json) : NULL;
  if (parsed && cJSON_IsObject(parsed)) {
    m->state = parsed; /* round-trip an existing document (SPEC §2) */
    vc_state_hydrate(m->state); /* a partial document gets the missing defaults */
  } else {
    if (parsed) cJSON_Delete(parsed); /* malformed/non-object => empty state */
    m->state = vc_state_new();
  }
  m->glyphs = vc_glyphs_new_builtin(); /* built-ins; host adds app glyphs after */
  return m;
}

int vc_register_glyph(VC_Manager *m, const char *glyph_json) {
  if (!m) return 0;
  return vc_glyph_register(m->glyphs, glyph_json);
}

void vc_set_log_sink(VC_Manager *m, VC_LogFn fn, void *user) {
  if (!m) return;
  m->log_sink = fn;
  m->log_user = user;
}

void vc_set_effect_handler(VC_Manager *m, VC_EffectFn fn, void *user) {
  if (!m) return;
  m->effect = fn;
  m->effect_user = user;
}

char *vc_dispatch(VC_Manager *m, const char *command) {
  if (!m) return NULL;
  cJSON *res = vc_dispatch_json(m, command ? command : "");
  char *out = cJSON_PrintUnformatted(res);
  cJSON_Delete(res);
  return out; /* heap-owned by cJSON's malloc; released via vc_free_str */
}

char *vc_export_state(VC_Manager *m) {
  if (!m) return NULL;
  return cJSON_PrintUnformatted(m->state);
}

char *vc_alloc_str(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d) memcpy(d, s, n);
  return d;
}

void vc_free_str(char *s) {
  if (s) free(s);
}

void vc_destroy(VC_Manager *m) {
  if (!m) return;
  vc_undo_clear(m);
  if (m->glyphs) cJSON_Delete(m->glyphs);
  if (m->log) cJSON_Delete(m->log);
  if (m->state) cJSON_Delete(m->state);
  free(m);
}

int vc_tag_match(const char *expr, const char *tags_json) {
  if (!expr || !tags_json) return -1;
  cJSON *tags = cJSON_Parse(tags_json);
  if (!cJSON_IsArray(tags)) {
    if (tags) cJSON_Delete(tags);
    return -1;
  }
  cJSON *it = NULL;
  cJSON_ArrayForEach(it, tags) {
    if (!cJSON_IsString(it)) { cJSON_Delete(tags); return -1; }
  }
  /* Wrap the bag of tags as a rune shape so the one evaluator (vc_filter_eval)
   * runs unchanged. spirit.name is empty: name-as-tag is the caller's choice
   * (put the name in the array). */
  cJSON *rune = cJSON_CreateObject();
  cJSON *spirit = cJSON_CreateObject();
  cJSON_AddStringToObject(spirit, "name", "");
  cJSON_AddItemToObject(rune, "spirit", spirit);
  cJSON_AddItemToObject(rune, "tags", tags);
  int r = vc_filter_eval(rune, expr) ? 1 : 0;
  cJSON_Delete(rune);
  return r;
}

const char *vc_version(void) { return VC_VERSION_STR; }
