/* rune.c — the atomic editable unit. SPEC §3.2.
 * `content` is created empty and stays opaque to the core: only the rune's glyph
 * (host-supplied, not wired yet) interprets it. */
#include "vc_internal.h"
#include <string.h>

static const char *kFacetKeys[6] = {"who", "what", "when", "where", "why", "how"};

static cJSON *default_facets(void) {
  cJSON *f = cJSON_CreateObject();
  for (int i = 0; i < 6; i++) cJSON_AddStringToObject(f, kFacetKeys[i], "");
  return f;
}

cJSON *vc_rune_new(const char *glyph, const char *name) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddItemToObject(r, "spirit", vc_spirit_new("rune", name));
  cJSON_AddStringToObject(r, "glyph", glyph && *glyph ? glyph : "text");
  cJSON_AddItemToObject(r, "facets", default_facets());
  cJSON_AddItemToObject(r, "tags", cJSON_CreateArray());
  cJSON_AddItemToObject(r, "content", cJSON_CreateObject());
  cJSON_AddItemToObject(r, "placement", cJSON_CreateNull());
  cJSON_AddItemToObject(r, "relations", cJSON_CreateArray());
  return r;
}

static const char *spirit_field(const cJSON *rune, const char *key) {
  cJSON *sp = cJSON_GetObjectItemCaseSensitive((cJSON *)rune, "spirit");
  cJSON *v = cJSON_GetObjectItemCaseSensitive(sp, key);
  return cJSON_IsString(v) ? v->valuestring : "";
}

const char *vc_rune_name(const cJSON *rune) { return spirit_field(rune, "name"); }
const char *vc_rune_id(const cJSON *rune) { return spirit_field(rune, "id"); }

int vc_rune_matches_ref(const cJSON *rune, const char *ref) {
  if (!ref) return 0;
  return strcmp(vc_rune_name(rune), ref) == 0 || strcmp(vc_rune_id(rune), ref) == 0;
}
