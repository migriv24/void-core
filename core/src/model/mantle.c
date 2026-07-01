/* mantle.c — runes over a domain, plus reserved layout/rules. SPEC §3.4.
 * layout.edges and rules are persisted from day one but not yet consumed. */
#include "vc_internal.h"
#include <string.h>

static const char *estr(const cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive((cJSON *)o, k);
  return cJSON_IsString(v) ? v->valuestring : "";
}

static cJSON *mantle_edges(cJSON *mantle) {
  cJSON *layout = cJSON_GetObjectItemCaseSensitive(mantle, "layout");
  if (!layout) {
    layout = cJSON_CreateObject();
    cJSON_AddItemToObject(layout, "edges", cJSON_CreateArray());
    cJSON_AddItemToObject(mantle, "layout", layout);
  }
  cJSON *edges = cJSON_GetObjectItemCaseSensitive(layout, "edges");
  if (!edges) {
    edges = cJSON_CreateArray();
    cJSON_AddItemToObject(layout, "edges", edges);
  }
  return edges;
}

cJSON *vc_mantle_new(const char *name, const char *domain) {
  char id[64];
  vc_mint_id("mantle", id, sizeof id);
  cJSON *m = cJSON_CreateObject();
  cJSON_AddStringToObject(m, "id", id);
  cJSON_AddStringToObject(m, "name", name ? name : "");
  if (domain) cJSON_AddStringToObject(m, "domain", domain);
  else cJSON_AddItemToObject(m, "domain", cJSON_CreateNull());
  cJSON_AddItemToObject(m, "runes", cJSON_CreateArray());
  cJSON_AddItemToObject(m, "tags", cJSON_CreateObject());
  cJSON *layout = cJSON_CreateObject();
  cJSON_AddItemToObject(layout, "edges", cJSON_CreateArray());
  cJSON_AddItemToObject(m, "layout", layout);
  cJSON_AddItemToObject(m, "rules", cJSON_CreateArray());
  return m;
}

const char *vc_mantle_name(const cJSON *mantle) {
  cJSON *nm = cJSON_GetObjectItemCaseSensitive((cJSON *)mantle, "name");
  return cJSON_IsString(nm) ? nm->valuestring : "";
}

cJSON *vc_mantle_runes(cJSON *mantle) {
  return cJSON_GetObjectItemCaseSensitive(mantle, "runes");
}

cJSON *vc_mantle_find_rune(cJSON *mantle, const char *ref) {
  cJSON *runes = vc_mantle_runes(mantle);
  cJSON *r = NULL;
  cJSON_ArrayForEach(r, runes) {
    if (vc_rune_matches_ref(r, ref)) return r;
  }
  return NULL;
}

int vc_mantle_add_rune(cJSON *mantle, cJSON *rune) {
  const char *name = vc_rune_name(rune);
  if (name && *name && vc_mantle_find_rune(mantle, name)) return 0; /* dup name */
  cJSON_AddItemToArray(vc_mantle_runes(mantle), rune);
  return 1;
}

int vc_mantle_remove_rune(cJSON *mantle, const char *ref) {
  cJSON *runes = vc_mantle_runes(mantle);
  int idx = 0;
  cJSON *r = NULL;
  cJSON_ArrayForEach(r, runes) {
    if (vc_rune_matches_ref(r, ref)) {
      char name[256];
      strncpy(name, vc_rune_name(r), sizeof name - 1);
      name[sizeof name - 1] = 0;
      cJSON_DeleteItemFromArray(runes, idx);
      /* SPEC §3.4: also drop any layout edges referencing the removed rune. */
      cJSON *edges = mantle_edges(mantle);
      int ei = 0;
      cJSON *e = NULL;
      while ((e = cJSON_GetArrayItem(edges, ei))) {
        if (!strcmp(estr(e, "from"), name) || !strcmp(estr(e, "to"), name))
          cJSON_DeleteItemFromArray(edges, ei);
        else
          ei++;
      }
      return 1;
    }
    idx++;
  }
  return 0;
}

static void set_num(cJSON *o, const char *k, double v) {
  cJSON_DeleteItemFromObjectCaseSensitive(o, k); /* no-op if absent */
  cJSON_AddNumberToObject(o, k, v);
}
static void set_bool(cJSON *o, const char *k, int v) {
  cJSON_DeleteItemFromObjectCaseSensitive(o, k);
  cJSON_AddBoolToObject(o, k, v ? 1 : 0);
}

cJSON *vc_mantle_add_edge(cJSON *mantle, const char *from, const char *to,
                          const char *relation, double weight, int directed) {
  cJSON *edges = mantle_edges(mantle);
  const char *rel = relation ? relation : "";
  cJSON *e = NULL;
  cJSON_ArrayForEach(e, edges) {
    if (!strcmp(estr(e, "from"), from) && !strcmp(estr(e, "to"), to) &&
        !strcmp(estr(e, "relation"), rel)) {
      set_num(e, "weight", weight); /* update existing link */
      set_bool(e, "directed", directed);
      return e;
    }
  }
  cJSON *ne = cJSON_CreateObject();
  cJSON_AddStringToObject(ne, "from", from);
  cJSON_AddStringToObject(ne, "to", to);
  cJSON_AddStringToObject(ne, "relation", rel);
  cJSON_AddNumberToObject(ne, "weight", weight);
  cJSON_AddBoolToObject(ne, "directed", directed ? 1 : 0);
  cJSON_AddItemToArray(edges, ne);
  return ne;
}

int vc_mantle_remove_edge(cJSON *mantle, const char *from, const char *to,
                          const char *relation) {
  cJSON *edges = mantle_edges(mantle);
  int removed = 0, i = 0;
  cJSON *e = NULL;
  while ((e = cJSON_GetArrayItem(edges, i))) {
    int match = !strcmp(estr(e, "from"), from) && !strcmp(estr(e, "to"), to) &&
                (!relation || !*relation || !strcmp(estr(e, "relation"), relation));
    if (match) {
      cJSON_DeleteItemFromArray(edges, i);
      removed++;
    } else {
      i++;
    }
  }
  return removed;
}
