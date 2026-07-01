/* spirit.c — a rune/mantle identity: a frozen real-ID + an editable name.
 * SPEC §3.1. */
#include "vc_internal.h"

cJSON *vc_spirit_new(const char *prefix, const char *name) {
  char id[64];
  vc_mint_id(prefix ? prefix : "rune", id, sizeof id);
  cJSON *s = cJSON_CreateObject();
  cJSON_AddStringToObject(s, "id", id);
  cJSON_AddStringToObject(s, "name", name ? name : "");
  return s;
}
