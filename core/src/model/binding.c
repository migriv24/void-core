/* binding.c — cross-mantle connections (SPEC §3.6). Bindings live at host level
 * (state.bindings) because they reference more than one mantle. The reaction
 * engine that *fires* a binding is reserved; this is the data model + resolution.
 */
#include "vc_internal.h"
#include <stdio.h>
#include <string.h>

cJSON *vc_bindings(cJSON *state) {
  cJSON *b = cJSON_GetObjectItemCaseSensitive(state, "bindings");
  if (!cJSON_IsArray(b)) {
    cJSON_DeleteItemFromObjectCaseSensitive(state, "bindings");
    b = cJSON_AddArrayToObject(state, "bindings");
  }
  return b;
}

/* Resolve "mantle:rune" or "rune" (mantle defaults to def_mantle) to the actual
 * mantle + rune objects. Returns 1 on success. */
int vc_parse_ref(cJSON *state, const char *ref, const char *def_mantle,
                 cJSON **mantle_out, cJSON **rune_out) {
  if (!ref) return 0;
  char mname[128], rname[128];
  const char *colon = strchr(ref, ':');
  if (colon) {
    size_t n = (size_t)(colon - ref);
    if (n >= sizeof mname) n = sizeof mname - 1;
    memcpy(mname, ref, n);
    mname[n] = 0;
    snprintf(rname, sizeof rname, "%s", colon + 1);
  } else {
    snprintf(mname, sizeof mname, "%s", def_mantle ? def_mantle : "");
    snprintf(rname, sizeof rname, "%s", ref);
  }
  cJSON *mm = NULL, *found = NULL;
  cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles"))
    if (!strcmp(vc_mantle_name(mm), mname)) { found = mm; break; }
  if (!found) return 0;
  cJSON *r = vc_mantle_find_rune(found, rname);
  if (!r) return 0;
  if (mantle_out) *mantle_out = found;
  if (rune_out) *rune_out = r;
  return 1;
}

cJSON *vc_binding_new(const char *name, cJSON *from, cJSON *to, const char *note) {
  char id[64];
  vc_mint_id("bind", id, sizeof id);
  cJSON *b = cJSON_CreateObject();
  cJSON_AddStringToObject(b, "id", id);
  if (name) cJSON_AddStringToObject(b, "name", name);
  else cJSON_AddItemToObject(b, "name", cJSON_CreateNull());
  cJSON_AddItemToObject(b, "from", from);
  cJSON_AddItemToObject(b, "to", to);
  cJSON_AddStringToObject(b, "note", note ? note : "");
  return b;
}
