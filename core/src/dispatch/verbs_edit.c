/* verbs_edit.c — the "edit" verb family for the dispatcher.
 * Handlers are grouped by family (SPEC §7); the router in dispatch.c tries each
 * family in turn. Bodies are unchanged from the original dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *vc_verbs_edit(VC_Manager *m, cJSON *state, vc_argv a, const char *v) {
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)m; (void)state; (void)a; (void)v; (void)err;
  if (0) {
  } else if (!strcmp(v, "use")) {
    if (a.count < 2 || !strcmp(a.items[1], "/")) {
      /* SPEC §7: `use` with no argument (or `/`) deactivates — back to the
       * mantle list, where root-`ls` shows what's there. */
      cJSON *active = cJSON_GetObjectItemCaseSensitive(state, "active");
      cJSON_ReplaceItemInObjectCaseSensitive(active, "mantle", cJSON_CreateNull());
      res = res_make(1);
      res_line(res, "no active mantle ('ls' lists mantles, 'use <mantle>' enters one)");
    } else {
      cJSON *found = NULL, *mm = NULL;
      cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles")) {
        if (!strcmp(vc_mantle_name(mm), a.items[1])) { found = mm; break; }
      }
      if (!found) {
        res = res_fail("no such mantle: %s", a.items[1]);
      } else {
        cJSON *active = cJSON_GetObjectItemCaseSensitive(state, "active");
        cJSON_ReplaceItemInObjectCaseSensitive(active, "mantle",
                                               cJSON_CreateString(a.items[1]));
        res = res_make(1);
        res_line(res, "active mantle: %s", a.items[1]);
      }
    }

  } else if (!strcmp(v, "mantle")) {
    if (a.count >= 3 && !strcmp(a.items[1], "new")) {
      const char *name = a.items[2];
      cJSON *mantles = cJSON_GetObjectItemCaseSensitive(state, "mantles");
      int dup = 0;
      cJSON *mm = NULL;
      cJSON_ArrayForEach(mm, mantles) {
        if (!strcmp(vc_mantle_name(mm), name)) { dup = 1; break; }
      }
      if (dup) {
        res = res_fail("mantle exists: %s", name);
      } else {
        cJSON_AddItemToArray(mantles, vc_mantle_new(name, NULL));
        cJSON *active = cJSON_GetObjectItemCaseSensitive(state, "active");
        cJSON_ReplaceItemInObjectCaseSensitive(active, "mantle",
                                               cJSON_CreateString(name));
        res = res_make(1);
        res_line(res, "created mantle: %s (active)", name);
      }
    } else {
      res = res_fail("usage: mantle new <name>");
    }

  } else if (!strcmp(v, "rune")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count >= 4 && !strcmp(a.items[1], "new")) {
      cJSON *gd = vc_glyph_find(m->glyphs, a.items[2]);
      if (!gd) {
        res = res_fail("unknown glyph: %s (try 'glyphs')", a.items[2]);
      } else {
        cJSON *r = vc_rune_new(a.items[2], a.items[3]);
        cJSON_ReplaceItemInObjectCaseSensitive(r, "content",
                                               vc_glyph_default_content(gd));
        if (!vc_mantle_add_rune(mt, r)) {
          cJSON_Delete(r);
          res = res_fail("rune name exists: %s", a.items[3]);
        } else {
          res = res_make(1);
          res_line(res, "created rune '%s' (glyph %s)", a.items[3], a.items[2]);
          res_set_data(res, cJSON_CreateString(a.items[3]));
        }
      }
    } else if (a.count >= 3 && !strcmp(a.items[1], "rm")) {
      const char *ref = a.items[2];
      if (ref[0] == '@') { /* remove every match (collect names first) */
        cJSON **tg;
        int n = collect_targets(mt, ref, &tg);
        if (n == 0) {
          res = res_fail("no rune matches: %s", ref);
        } else {
          char **names = (char **)malloc((size_t)n * sizeof(char *));
          for (int i = 0; i < n; i++) names[i] = vc_strdup(vc_rune_name(tg[i]));
          res = res_make(1);
          for (int i = 0; i < n; i++) {
            vc_mantle_remove_rune(mt, names[i]);
            res_line(res, "removed %s", names[i]);
            free(names[i]);
          }
          free(names);
        }
        free(tg);
      } else {
        if (vc_mantle_remove_rune(mt, ref)) {
          res = res_make(1);
          res_line(res, "removed %s", ref);
        } else {
          res = res_fail("no such rune: %s", ref);
        }
      }
    } else if (a.count >= 4 && !strcmp(a.items[1], "rename")) {
      cJSON *r = vc_mantle_find_rune(mt, a.items[2]);
      if (!r) {
        res = res_fail("no such rune: %s", a.items[2]);
      } else if (vc_mantle_find_rune(mt, a.items[3])) {
        res = res_fail("name taken: %s", a.items[3]);
      } else {
        char old[256];
        strncpy(old, vc_rune_name(r), sizeof old - 1);
        old[sizeof old - 1] = 0;
        cJSON *sp = cJSON_GetObjectItemCaseSensitive(r, "spirit");
        cJSON_ReplaceItemInObjectCaseSensitive(sp, "name",
                                               cJSON_CreateString(a.items[3]));
        /* SPEC §3.4: repoint layout edges referencing the old name. */
        cJSON *layout = cJSON_GetObjectItemCaseSensitive(mt, "layout");
        cJSON *edges = layout ? cJSON_GetObjectItemCaseSensitive(layout, "edges") : NULL;
        cJSON *e = NULL;
        cJSON_ArrayForEach(e, edges) {
          cJSON *fr = cJSON_GetObjectItemCaseSensitive(e, "from");
          cJSON *to = cJSON_GetObjectItemCaseSensitive(e, "to");
          if (cJSON_IsString(fr) && !strcmp(fr->valuestring, old))
            cJSON_ReplaceItemInObjectCaseSensitive(e, "from", cJSON_CreateString(a.items[3]));
          if (cJSON_IsString(to) && !strcmp(to->valuestring, old))
            cJSON_ReplaceItemInObjectCaseSensitive(e, "to", cJSON_CreateString(a.items[3]));
        }
        /* SPEC §3.4: repoint name-tag references in other runes' tags too. */
        cJSON *rr = NULL;
        cJSON_ArrayForEach(rr, cJSON_GetObjectItemCaseSensitive(mt, "runes")) {
          cJSON *rtags = cJSON_GetObjectItemCaseSensitive(rr, "tags");
          int nt = cJSON_GetArraySize(rtags);
          for (int ti = 0; ti < nt; ti++) {
            cJSON *tg2 = cJSON_GetArrayItem(rtags, ti);
            if (cJSON_IsString(tg2) && !strcmp(tg2->valuestring, old))
              cJSON_ReplaceItemInArray(rtags, ti, cJSON_CreateString(a.items[3]));
          }
        }
        res = res_make(1);
        res_line(res, "renamed %s -> %s", a.items[2], a.items[3]);
      }
    } else if (a.count >= 5 && !strcmp(a.items[1], "move")) {
      /* rune move <ref> <relation> <target> -> set a layout edge (SPEC §3.4/§7). */
      cJSON *from = vc_mantle_find_rune(mt, a.items[2]);
      cJSON *to = vc_mantle_find_rune(mt, a.items[4]);
      if (!from) {
        res = res_fail("no such rune: %s", a.items[2]);
      } else if (!to) {
        res = res_fail("no such rune: %s", a.items[4]);
      } else {
        vc_mantle_add_edge(mt, vc_rune_name(from), vc_rune_name(to), a.items[3],
                           1.0, 1);
        res = res_make(1);
        res_line(res, "edge %s -%s-> %s", vc_rune_name(from), a.items[3],
                 vc_rune_name(to));
      }
    } else if (a.count >= 3 && !strcmp(a.items[1], "dup")) {
      /* rune dup <ref> [<newname>] -> copy with a fresh identity (SPEC §7). */
      cJSON *src = vc_mantle_find_rune(mt, a.items[2]);
      if (!src) {
        res = res_fail("no such rune: %s", a.items[2]);
      } else {
        char newname[256];
        if (a.count >= 4) {
          strncpy(newname, a.items[3], sizeof newname - 1);
          newname[sizeof newname - 1] = 0;
        } else {
          snprintf(newname, sizeof newname, "%s-copy", vc_rune_name(src));
        }
        if (vc_mantle_find_rune(mt, newname)) {
          res = res_fail("name taken: %s", newname);
        } else {
          cJSON *copy = cJSON_Duplicate(src, 1);
          cJSON_ReplaceItemInObjectCaseSensitive(copy, "spirit",
                                                 vc_spirit_new("rune", newname));
          vc_mantle_add_rune(mt, copy);
          res = res_make(1);
          res_line(res, "duplicated %s -> %s", vc_rune_name(src), newname);
          res_set_data(res, cJSON_CreateString(newname));
        }
      }
    } else {
      res = res_fail("usage: rune new <glyph> <name> | rune rm <ref> | "
                     "rune rename <ref> <new> | rune move <ref> <relation> <target> | "
                     "rune dup <ref> [<new>]");
    }

  } else if (!strcmp(v, "set")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 4) {
      res = res_fail("usage: set <ref> <field> <value>");
    } else {
      cJSON **tg;
      int n = collect_targets(mt, a.items[1], &tg);
      if (n == 0) {
        res = res_fail("no rune matches: %s", a.items[1]);
      } else {
        res = res_make(1);
        for (int i = 0; i < n; i++) {
          cJSON *content = cJSON_GetObjectItemCaseSensitive(tg[i], "content");
          cJSON_DeleteItemFromObjectCaseSensitive(content, a.items[2]);
          cJSON_AddStringToObject(content, a.items[2], a.items[3]);
          res_line(res, "%s.%s = %s", vc_rune_name(tg[i]), a.items[2], a.items[3]);
        }
      }
      free(tg);
    }

  } else if (!strcmp(v, "setjson")) {
    /* like `set`, but the value is parsed as JSON (number/bool/array/object/
     * string); invalid JSON falls back to a plain string. This is how a host/UI
     * sets typed or structured content through the dispatcher. */
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 4) {
      res = res_fail("usage: setjson <ref> <field> <json-value>");
    } else {
      cJSON **tg;
      int n = collect_targets(mt, a.items[1], &tg);
      if (n == 0) {
        res = res_fail("no rune matches: %s", a.items[1]);
      } else {
        cJSON *parsed = cJSON_Parse(a.items[3]); /* NULL => use string fallback */
        res = res_make(1);
        for (int i = 0; i < n; i++) {
          cJSON *content = cJSON_GetObjectItemCaseSensitive(tg[i], "content");
          cJSON_DeleteItemFromObjectCaseSensitive(content, a.items[2]);
          cJSON *val = parsed ? cJSON_Duplicate(parsed, 1)
                              : cJSON_CreateString(a.items[3]);
          cJSON_AddItemToObject(content, a.items[2], val);
          res_line(res, "%s.%s = %s", vc_rune_name(tg[i]), a.items[2], a.items[3]);
        }
        if (parsed) cJSON_Delete(parsed);
      }
      free(tg);
    }

  } else if (!strcmp(v, "facet")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 4) {
      res = res_fail("usage: facet <ref> <who|what|when|where|why|how> <value>");
    } else {
      int valid = 0;
      for (int i = 0; i < 6; i++)
        if (!strcmp(a.items[2], vc_facet_keys[i])) valid = 1;
      if (!valid) {
        res = res_fail("not a facet: %s", a.items[2]);
      } else {
        cJSON **tg;
        int n = collect_targets(mt, a.items[1], &tg);
        if (n == 0) {
          res = res_fail("no rune matches: %s", a.items[1]);
        } else {
          res = res_make(1);
          for (int i = 0; i < n; i++) {
            cJSON *f = cJSON_GetObjectItemCaseSensitive(tg[i], "facets");
            cJSON_DeleteItemFromObjectCaseSensitive(f, a.items[2]);
            cJSON_AddStringToObject(f, a.items[2], a.items[3]);
            res_line(res, "facet %s.%s set", vc_rune_name(tg[i]), a.items[2]);
          }
        }
        free(tg);
      }
    }

  } else if (!strcmp(v, "tag")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 3) {
      res = res_fail("usage: tag <ref> +add -remove ...");
    } else {
      cJSON **tg;
      int n = collect_targets(mt, a.items[1], &tg);
      if (n == 0) {
        res = res_fail("no rune matches: %s", a.items[1]);
      } else {
        res = res_make(1);
        for (int k = 0; k < n; k++) {
          cJSON *tags = cJSON_GetObjectItemCaseSensitive(tg[k], "tags");
          const char *nm = vc_rune_name(tg[k]);
          for (int i = 2; i < a.count; i++) {
            const char *t = a.items[i];
            if (t[0] == '+' && t[1]) {
              const char *tag = t + 1;
              int present = 0; /* add only if not already an explicit tag */
              cJSON *it = NULL;
              cJSON_ArrayForEach(it, tags) {
                if (cJSON_IsString(it) && !strcmp(it->valuestring, tag)) {
                  present = 1;
                  break;
                }
              }
              if (!present) {
                cJSON_AddItemToArray(tags, cJSON_CreateString(tag));
                res_line(res, "%s +%s", nm, tag);
              }
            } else if (t[0] == '-' && t[1]) {
              const char *tag = t + 1;
              int idx = 0;
              cJSON *it = NULL;
              cJSON_ArrayForEach(it, tags) {
                if (cJSON_IsString(it) && !strcmp(it->valuestring, tag)) {
                  cJSON_DeleteItemFromArray(tags, idx);
                  res_line(res, "%s -%s", nm, tag);
                  break;
                }
                idx++;
              }
            } else {
              res_line(res, "(ignored '%s'; use +tag or -tag)", t);
            }
          }
        }
      }
      free(tg);
    }

  }
  return res;
}
