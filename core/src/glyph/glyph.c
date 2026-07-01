/* glyph.c — the glyph registry (SPEC §3.3).
 *
 * A glyph is a rune's intrinsic *type* and the seam where a host app declares how
 * a rune is edited/described/rendered. v0 glyphs are data descriptors
 * { glyph, label, editor, fields[] }; host *callbacks* (describe/newContent/render)
 * attach in a later slice when the FFI callback bridge lands.
 *
 * The registry is host-supplied app config, held on the manager — it is NOT part
 * of the exported state document. A reloaded manager must re-register app glyphs
 * (built-ins are always present). Per the brainstorm, a rune's glyph is meant to
 * also be queryable as the reserved tag `glyph:<name>` once the tag-filter grammar
 * exists (next slice).
 */
#include "vc_internal.h"
#include <string.h>

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
  return reg;
}

cJSON *vc_glyph_find(cJSON *glyphs, const char *name) {
  if (!glyphs || !name) return NULL;
  return cJSON_GetObjectItemCaseSensitive(glyphs, name);
}

int vc_glyph_register(cJSON *glyphs, const char *glyph_json) {
  if (!glyphs || !glyph_json) return 0;
  cJSON *def = cJSON_Parse(glyph_json);
  if (!def || !cJSON_IsObject(def)) {
    if (def) cJSON_Delete(def);
    return 0;
  }
  cJSON *nm = cJSON_GetObjectItemCaseSensitive(def, "glyph");
  if (!cJSON_IsString(nm) || !*nm->valuestring) {
    cJSON_Delete(def);
    return 0;
  }
  if (!cJSON_GetObjectItemCaseSensitive(def, "fields"))
    cJSON_AddItemToObject(def, "fields", cJSON_CreateArray());
  cJSON_DeleteItemFromObjectCaseSensitive(glyphs, nm->valuestring); /* override */
  cJSON_AddItemToObject(glyphs, nm->valuestring, def);              /* takes ownership */
  return 1;
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
