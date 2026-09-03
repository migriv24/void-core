/* glyph.c — the glyph registry (SPEC §3.3).
 *
 * A glyph declares what a rune IS: its content `fields`, an optional `kind`
 * (§3.3.1), optional per-field quantity annotations (`kinds`, §3.3.2), and — for
 * hosts that render — an optional `presentations` map keyed by modality.
 *
 * ── The split this file now carries (Void Hormiga, 2026-09-03) ──────────────
 * A descriptor used to answer two questions with one object: "what is this
 * rune?" (the schema — true in every mantle, for every output, forever) and
 * "how does this rune appear?" (the presentation — one per modality). Those are
 * in bijection only while a rune has exactly one representation; the moment it
 * has a sprite AND a sound AND a table row, presentation stops being a property
 * of the rune and becomes a function from the rune to a modality. The two are
 * now separate objects on the descriptor: `fields`/`kind`/`kinds` are the
 * schema, `presentations` is that function. `editor`, `label` and any host
 * `hints` stay exactly where they are — this is additive, and the eventual
 * rename is deliberately NOT done here (see okf/concepts/glyph.md).
 *
 * ── Two registries, one lookup (SPEC §2, §3.3.3) ───────────────────────────
 * Descriptors reach a manager two ways:
 *   - DECLARED, in `state.glyphs` — part of the state document, so a bundle
 *     carries its runes AND their meaning. Written by `glyph declare`, an
 *     ordinary logged, journaled, undoable command.
 *   - REGISTERED, on the manager — the built-ins plus whatever the host passes
 *     to vc_register_glyph(). Host configuration; NOT exported.
 * A declaration SHADOWS a registration of the same name, because the
 * declaration is the one that traveled with the data. `glyphs` reports which
 * source each descriptor came from, so the shadowing is never silent.
 */
#include "vc_internal.h"
#include <stdio.h>
#include <string.h>

/* ── the three rune kinds (SPEC §3.3.1) ───────────────────────────────────── */
const char *const vc_rune_kinds[3] = {"entity", "act", "measure"};

static int is_rune_kind(const char *s) {
  for (int i = 0; i < 3; i++)
    if (!strcmp(s, vc_rune_kinds[i])) return 1;
  return 0;
}

/* ── measurement levels (SPEC §3.3.2) ─────────────────────────────────────── */
static int is_level(const char *s) {
  return !strcmp(s, "nominal") || !strcmp(s, "ordinal") ||
         !strcmp(s, "interval") || !strcmp(s, "ratio");
}

static void add_builtin(cJSON *reg, const char *name, const char *label,
                        const char *editor, const char **fields, int nf) {
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "glyph", name);
  cJSON_AddStringToObject(d, "label", label);
  cJSON_AddStringToObject(d, "editor", editor);
  cJSON *fa = cJSON_CreateArray();
  for (int i = 0; i < nf; i++) cJSON_AddItemToArray(fa, cJSON_CreateString(fields[i]));
  cJSON_AddItemToObject(d, "fields", fa);
  cJSON_AddItemToObject(reg, name, d); /* key = glyph name */
}

cJSON *vc_glyphs_new_builtin(void) {
  cJSON *reg = cJSON_CreateObject();
  const char *f_value[] = {"value"};
  const char *f_html[] = {"html"};
  const char *f_image[] = {"src", "alt"};
  const char *f_items[] = {"items"};
  const char *f_link[] = {"href", "label"};
  const char *f_children[] = {"children"};
  add_builtin(reg, "text", "Text block", "text", f_value, 1);
  add_builtin(reg, "richtext", "Rich text", "richtext", f_html, 1);
  add_builtin(reg, "image", "Image", "image", f_image, 2);
  add_builtin(reg, "imageList", "Image list", "imageList", f_items, 1);
  add_builtin(reg, "color", "Color", "color", f_value, 1);
  add_builtin(reg, "link", "Link", "link", f_link, 2);
  add_builtin(reg, "group", "Group", "group", f_children, 1);
  /* Every built-in is an `entity` by the default — the kind is omitted rather
   * than written, so a descriptor that predates kinds and one that declines to
   * state a kind read identically. Nothing migrates (SPEC §3.3.1). */
  return reg;
}

cJSON *vc_glyph_find(cJSON *glyphs, const char *name) {
  if (!glyphs || !name) return NULL;
  return cJSON_GetObjectItemCaseSensitive(glyphs, name);
}

cJSON *vc_glyphs_declared(cJSON *state) {
  cJSON *g = cJSON_GetObjectItemCaseSensitive(state, "glyphs");
  if (!cJSON_IsObject(g)) {
    if (g) cJSON_DeleteItemFromObjectCaseSensitive(state, "glyphs");
    g = cJSON_CreateObject();
    cJSON_AddItemToObject(state, "glyphs", g);
  }
  return g;
}

cJSON *vc_glyph_lookup(VC_Manager *m, const char *name) {
  if (!m || !name) return NULL;
  cJSON *d = vc_glyph_find(cJSON_GetObjectItemCaseSensitive(m->state, "glyphs"), name);
  return d ? d : vc_glyph_find(m->glyphs, name);
}

const char *vc_glyph_source(VC_Manager *m, const char *name) {
  if (!m || !name) return NULL;
  if (vc_glyph_find(cJSON_GetObjectItemCaseSensitive(m->state, "glyphs"), name))
    return "document";
  if (vc_glyph_find(m->glyphs, name)) return "host";
  return NULL;
}

const char *vc_glyph_kind(const cJSON *glyphdef) {
  cJSON *k = cJSON_GetObjectItemCaseSensitive((cJSON *)glyphdef, "kind");
  if (cJSON_IsString(k) && is_rune_kind(k->valuestring)) return k->valuestring;
  return "entity"; /* the default, and what every rune that exists today is */
}

cJSON *vc_glyph_field_kind(const cJSON *glyphdef, const char *field) {
  cJSON *kinds = cJSON_GetObjectItemCaseSensitive((cJSON *)glyphdef, "kinds");
  if (!cJSON_IsObject(kinds) || !field) return NULL;
  return cJSON_GetObjectItemCaseSensitive(kinds, field);
}

/* A quantity annotation — `{level, unit, min, max}` — wherever one appears: on a
 * glyph field (`kinds`, §3.3.2) or on a measure rune (`quantity`, §3.2). One
 * shape, one validator, one concept page (okf/concepts/quantity.md), because an
 * application that puts a value in a field and one that puts it on an edge are
 * describing the same quantity and must not have to say it two ways. */
int vc_quantity_validate(const cJSON *q, char *err, size_t errsz) {
#define VC_QBAD(...) do { if (err) snprintf(err, errsz, __VA_ARGS__); return 0; } while (0)
  if (!cJSON_IsObject((cJSON *)q))
    VC_QBAD("a quantity annotation must be an object "
            "{\"level\":...,\"unit\":...,\"min\":...,\"max\":...}");
  cJSON *lv = cJSON_GetObjectItemCaseSensitive((cJSON *)q, "level");
  if (lv && (!cJSON_IsString(lv) || !is_level(lv->valuestring)))
    VC_QBAD("\"level\" must be one of nominal|ordinal|interval|ratio "
            "(what a number IS is a question of which operations are legal)");
  cJSON *un = cJSON_GetObjectItemCaseSensitive((cJSON *)q, "unit");
  if (un && !cJSON_IsString(un)) VC_QBAD("\"unit\" must be a string");
  const char *bounds[2] = {"min", "max"};
  for (int i = 0; i < 2; i++) {
    cJSON *b = cJSON_GetObjectItemCaseSensitive((cJSON *)q, bounds[i]);
    if (b && !cJSON_IsNumber(b) && !cJSON_IsNull(b))
      VC_QBAD("\"%s\" must be a number or null", bounds[i]);
  }
  return 1;
#undef VC_QBAD
}

/* ── validation (SPEC §3.3) ────────────────────────────────────────────────
 *
 * A descriptor is app config that OUTLIVES the session that wrote it and — once
 * declared — travels to hosts that never saw the code that produced it. A typo
 * in it is therefore not a local mistake; it is a fact about a type that nobody
 * downstream can question. So the shape is checked at the door and refused with
 * a sentence, rather than stored and silently ignored at every later read. */
int vc_glyph_validate(const cJSON *def, char *err, size_t errsz) {
#define VC_GBAD(...) do { if (err) snprintf(err, errsz, __VA_ARGS__); return 0; } while (0)
  if (!cJSON_IsObject((cJSON *)def)) VC_GBAD("descriptor must be a JSON object");
  cJSON *nm = cJSON_GetObjectItemCaseSensitive((cJSON *)def, "glyph");
  if (!cJSON_IsString(nm) || !*nm->valuestring)
    VC_GBAD("descriptor needs a non-empty string \"glyph\" (its name)");

  cJSON *fields = cJSON_GetObjectItemCaseSensitive((cJSON *)def, "fields");
  if (fields && !cJSON_IsArray(fields))
    VC_GBAD("\"fields\" must be an array of content field names");
  cJSON *f = NULL;
  cJSON_ArrayForEach(f, fields) {
    if (!cJSON_IsString(f) || !*f->valuestring)
      VC_GBAD("\"fields\" must hold non-empty strings");
  }

  cJSON *kind = cJSON_GetObjectItemCaseSensitive((cJSON *)def, "kind");
  if (kind && (!cJSON_IsString(kind) || !is_rune_kind(kind->valuestring)))
    VC_GBAD("\"kind\" must be one of entity|act|measure (default entity)");

  cJSON *kinds = cJSON_GetObjectItemCaseSensitive((cJSON *)def, "kinds");
  if (kinds && !cJSON_IsObject(kinds))
    VC_GBAD("\"kinds\" must be an object keyed by field name");
  cJSON *ent = NULL;
  cJSON_ArrayForEach(ent, kinds) {
    const char *fname = ent->string ? ent->string : "";
    /* A `kinds` key that names no declared field is a typo whose only symptom
     * would be an annotation nobody ever reads. */
    int known = 0;
    cJSON_ArrayForEach(f, fields)
      if (cJSON_IsString(f) && !strcmp(f->valuestring, fname)) { known = 1; break; }
    if (!known)
      VC_GBAD("\"kinds\" annotates '%s', which is not one of this glyph's fields",
              fname);
    if (!vc_quantity_validate(ent, err, errsz)) return 0;
  }

  cJSON *pres = cJSON_GetObjectItemCaseSensitive((cJSON *)def, "presentations");
  if (pres && !cJSON_IsObject(pres))
    VC_GBAD("\"presentations\" must be an object keyed by modality "
            "(e.g. canvas, audio, email)");
  cJSON *p = NULL;
  cJSON_ArrayForEach(p, pres) {
    if (!cJSON_IsObject(p))
      VC_GBAD("presentation '%s' must be an object", p->string ? p->string : "?");
  }
  return 1;
#undef VC_GBAD
}

int vc_glyph_register_err(cJSON *glyphs, const char *glyph_json, char *err,
                          size_t errsz) {
  if (!glyphs || !glyph_json) {
    if (err) snprintf(err, errsz, "no descriptor given");
    return 0;
  }
  cJSON *def = cJSON_Parse(glyph_json);
  if (!def) {
    if (err) snprintf(err, errsz, "descriptor is not valid JSON");
    return 0;
  }
  if (!vc_glyph_validate(def, err, errsz)) {
    cJSON_Delete(def);
    return 0;
  }
  cJSON *nm = cJSON_GetObjectItemCaseSensitive(def, "glyph");
  if (!cJSON_GetObjectItemCaseSensitive(def, "fields"))
    cJSON_AddItemToObject(def, "fields", cJSON_CreateArray());
  cJSON_DeleteItemFromObjectCaseSensitive(glyphs, nm->valuestring); /* override */
  cJSON_AddItemToObject(glyphs, nm->valuestring, def);              /* takes ownership */
  return 1;
}

int vc_glyph_register(cJSON *glyphs, const char *glyph_json) {
  char err[256];
  return vc_glyph_register_err(glyphs, glyph_json, err, sizeof err);
}

/* The descriptor as a HOST CONTRACT (SPEC §3.3.3): the stored object plus the
 * defaults resolved, so a host reads ONE shape whether or not the author wrote
 * every key. `source` says which registry answered. This is the object that lets
 * a host delete its hand-maintained second copy of the schema. */
cJSON *vc_glyph_resolved(VC_Manager *m, const cJSON *def, const char *name) {
  cJSON *out = cJSON_Duplicate((cJSON *)def, 1);
  if (!cJSON_GetObjectItemCaseSensitive(out, "glyph"))
    cJSON_AddStringToObject(out, "glyph", name ? name : "");
  if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(out, "fields"))) {
    cJSON_DeleteItemFromObjectCaseSensitive(out, "fields");
    cJSON_AddItemToObject(out, "fields", cJSON_CreateArray());
  }
  cJSON_DeleteItemFromObjectCaseSensitive(out, "kind");
  cJSON_AddStringToObject(out, "kind", vc_glyph_kind(def));
  const char *src = vc_glyph_source(m, name ? name : "");
  cJSON_DeleteItemFromObjectCaseSensitive(out, "source");
  cJSON_AddStringToObject(out, "source", src ? src : "host");
  return out;
}

cJSON *vc_glyph_default_content(const cJSON *glyphdef) {
  cJSON *content = cJSON_CreateObject();
  cJSON *fields = cJSON_GetObjectItemCaseSensitive((cJSON *)glyphdef, "fields");
  cJSON *f = NULL;
  cJSON_ArrayForEach(f, fields) {
    if (cJSON_IsString(f)) cJSON_AddStringToObject(content, f->valuestring, "");
  }
  return content;
}
