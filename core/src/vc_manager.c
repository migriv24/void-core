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

const char *vc_version(void) { return "0.1.0"; }
