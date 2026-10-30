/* mantle.c — runes over a domain, plus reserved layout/rules. SPEC §3.4.
 * layout.edges and rules are persisted from day one but not yet consumed. */
#include "vc_internal.h"
#include <stdlib.h>
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

/* ── vc_smap: string -> pointer, open addressing, borrowed keys ── */

static size_t smap_hash(const char *s) { /* FNV-1a */
  size_t h = (size_t)1469598103934665603ULL;
  for (; *s; s++) h = (h ^ (unsigned char)*s) * (size_t)1099511628211ULL;
  return h;
}

static vc_smap_slot *smap_slot(vc_smap_slot *slots, size_t cap, const char *key) {
  size_t i = smap_hash(key) & (cap - 1);
  while (slots[i].key && strcmp(slots[i].key, key)) i = (i + 1) & (cap - 1);
  return &slots[i];
}

void vc_smap_free(vc_smap *s) {
  free(s->slots);
  s->slots = NULL;
  s->cap = s->used = 0;
}

void vc_smap_reset(vc_smap *s) {
  if (s->slots) memset(s->slots, 0, s->cap * sizeof *s->slots);
  s->used = 0;
}

void *vc_smap_get(const vc_smap *s, const char *key) {
  if (!s->cap || !key) return NULL;
  return smap_slot(s->slots, s->cap, key)->val;
}

int vc_smap_put_new(vc_smap *s, const char *key, void *val) {
  if (!key) return 0;
  if ((s->used + 1) * 10 >= s->cap * 7) { /* keep load under 0.7 */
    size_t ncap = s->cap ? s->cap * 2 : 64;
    vc_smap_slot *ns = (vc_smap_slot *)calloc(ncap, sizeof *ns);
    if (!ns) return -1;
    for (size_t i = 0; i < s->cap; i++)
      if (s->slots[i].key) *smap_slot(ns, ncap, s->slots[i].key) = s->slots[i];
    free(s->slots);
    s->slots = ns;
    s->cap = ncap;
  }
  vc_smap_slot *sl = smap_slot(s->slots, s->cap, key);
  if (sl->key) return 0; /* first insert wins */
  sl->key = key;
  sl->val = val;
  s->used++;
  return 1;
}

/* ── the ref index ──
 * A rune answers a ref by its name OR its id (vc_rune_matches_ref), and the scan
 * returns the FIRST match in document order. Inserting each rune's name and then
 * its id, in document order, with first-insert-wins, makes the lookup return
 * exactly that rune — including the odd document where one rune's name is
 * another's id. The index is an answer cache, never a second source of truth. */

/* 0 if out of memory — the caller then drops the index, because an index
 * missing a key would answer "no such rune" for a rune that exists. */
static int rix_insert(VC_Manager *m, cJSON *rune) {
  return vc_smap_put_new(&m->rix, vc_rune_name(rune), rune) >= 0 &&
         vc_smap_put_new(&m->rix, vc_rune_id(rune), rune) >= 0;
}

void vc_rune_index_drop(VC_Manager *m) {
  if (!m) return;
  m->rix_mantle = NULL;
  vc_smap_reset(&m->rix);
}

cJSON *vc_find_rune(VC_Manager *m, cJSON *mantle, const char *ref) {
  if (!m || !m->rix_live || !mantle || !ref) return vc_mantle_find_rune(mantle, ref);
  if (m->rix_mantle != mantle) {
    vc_rune_index_drop(m);
    cJSON *r = NULL;
    cJSON_ArrayForEach(r, vc_mantle_runes(mantle)) {
      if (!rix_insert(m, r)) {
        vc_rune_index_drop(m);
        return vc_mantle_find_rune(mantle, ref);
      }
    }
    m->rix_mantle = mantle;
  }
  return (cJSON *)vc_smap_get(&m->rix, ref);
}

void vc_rune_index_add(VC_Manager *m, cJSON *mantle, cJSON *rune) {
  if (m && m->rix_mantle && m->rix_mantle == mantle && !rix_insert(m, rune))
    vc_rune_index_drop(m);
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
