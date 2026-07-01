/* verbs_query.c — the "query" verb family for the dispatcher.
 * Handlers are grouped by family (SPEC §7); the router in dispatch.c tries each
 * family in turn. Bodies are unchanged from the original dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *vc_verbs_query(VC_Manager *m, cJSON *state, vc_argv a, const char *v) {
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)m; (void)state; (void)a; (void)v; (void)err;
  if (0) {
  } else if (!strcmp(v, "version")) {
    res = res_make(1);
    res_line(res, "Void Core %s", VC_VERSION_STR);
    res_set_data(res, cJSON_CreateString(VC_VERSION_STR));

  } else if (!strcmp(v, "help")) {
    const char *verbs =
        "version help glyphs mantles mantle use where rune ls find describe get "
        "set setjson tag facet axes cat tree validate export undo redo history "
        "status diff revert save build deploy preview effect log "
        "bind bindings unbind batch relate unrelate related rule script";
    res = res_make(1);
    res_line(res, "verbs: %s", verbs);
    res_set_data(res, cJSON_CreateString(verbs));

  } else if (!strcmp(v, "mantles")) {
    res = res_make(1);
    cJSON *arr = cJSON_CreateArray();
    cJSON *active = vc_active_mantle(state);
    cJSON *mm = NULL;
    cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles")) {
      const char *nm = vc_mantle_name(mm);
      res_line(res, "%s%s", mm == active ? "* " : "  ", nm);
      cJSON_AddItemToArray(arr, cJSON_CreateString(nm));
    }
    if (cJSON_GetArraySize(arr) == 0) res_line(res, "(no mantles)");
    res_set_data(res, arr);

  } else if (!strcmp(v, "where")) {
    res = res_make(1);
    cJSON *mt = vc_active_mantle(state);
    cJSON *active = cJSON_GetObjectItemCaseSensitive(state, "active");
    cJSON *dom = active ? cJSON_GetObjectItemCaseSensitive(active, "domain") : NULL;
    res_line(res, "mantle: %s", mt ? vc_mantle_name(mt) : "(none)");
    res_line(res, "domain: %s", cJSON_IsString(dom) ? dom->valuestring : "(none)");

  } else if (!strcmp(v, "glyphs")) {
    res = res_make(1);
    cJSON *arr = cJSON_CreateArray();
    cJSON *gd = NULL;
    cJSON_ArrayForEach(gd, m->glyphs) {
      const char *nm = gd->string ? gd->string : gstr(gd, "glyph");
      res_line(res, "%-12s %s", nm, gstr(gd, "label"));
      cJSON_AddItemToArray(arr, cJSON_Duplicate(gd, 1));
    }
    res_set_data(res, arr);

  } else if (!strcmp(v, "ls")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else {
      /* optional "--tag <expr>": join all tokens after the flag so an unquoted
       * multi-word expression still works. */
      char expr[1024];
      expr[0] = 0;
      int have_expr = 0;
      for (int i = 1; i < a.count; i++) {
        if (!strcmp(a.items[i], "--tag")) {
          have_expr = 1;
          for (int j = i + 1; j < a.count; j++) {
            if (j > i + 1) strncat(expr, " ", sizeof expr - strlen(expr) - 1);
            strncat(expr, a.items[j], sizeof expr - strlen(expr) - 1);
          }
          break;
        }
      }
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, vc_mantle_runes(mt)) {
        if (have_expr && !vc_filter_eval(r, expr)) continue;
        const char *nm = vc_rune_name(r);
        res_line(res, "%s", nm);
        cJSON_AddItemToArray(arr, cJSON_CreateString(nm));
      }
      if (cJSON_GetArraySize(arr) == 0)
        res_line(res, have_expr ? "(no matches)" : "(empty)");
      res_set_data(res, arr);
    }

  } else if (!strcmp(v, "describe")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_make(1);
      res_line(res, "mantle '%s': %d rune(s)", vc_mantle_name(mt),
               cJSON_GetArraySize(vc_mantle_runes(mt)));
    } else {
      cJSON *r = vc_mantle_find_rune(mt, a.items[1]);
      if (!r) {
        res = res_fail("no such rune: %s", a.items[1]);
      } else {
        res = res_make(1);
        res_line(res, "%s  [glyph %s]  id=%s", vc_rune_name(r), gstr(r, "glyph"),
                 vc_rune_id(r));
        cJSON *f = cJSON_GetObjectItemCaseSensitive(r, "facets");
        for (int i = 0; i < 6; i++) {
          const char *fv = gstr(f, vc_facet_keys[i]);
          res_line(res, "  %-6s %s", vc_facet_keys[i], *fv ? fv : "-");
        }
        char tagbuf[512];
        tagbuf[0] = 0;
        int first = 1;
        cJSON *it = NULL;
        cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(r, "tags")) {
          if (!cJSON_IsString(it)) continue;
          if (!first) strncat(tagbuf, ", ", sizeof tagbuf - strlen(tagbuf) - 1);
          strncat(tagbuf, it->valuestring, sizeof tagbuf - strlen(tagbuf) - 1);
          first = 0;
        }
        res_line(res, "  tags   %s", first ? "-" : tagbuf);
        res_set_data(res, cJSON_Duplicate(r, 1));
      }
    }

  } else if (!strcmp(v, "get")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_fail("usage: get <ref> [field]");
    } else {
      cJSON *r = vc_mantle_find_rune(mt, a.items[1]);
      if (!r) {
        res = res_fail("no such rune: %s", a.items[1]);
      } else {
        cJSON *content = cJSON_GetObjectItemCaseSensitive(r, "content");
        cJSON *target = content;
        if (a.count >= 3) {
          target = cJSON_GetObjectItemCaseSensitive(content, a.items[2]);
          if (!target) {
            res = res_fail("no content field: %s", a.items[2]);
          }
        }
        if (!res) {
          res = res_make(1);
          char *s = cJSON_PrintUnformatted(target);
          res_line(res, "%s", s ? s : "");
          free(s);
          res_set_data(res, cJSON_Duplicate(target, 1));
        }
      }
    }

  } else if (!strcmp(v, "cat")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_fail("usage: cat <ref>");
    } else {
      cJSON *r = vc_mantle_find_rune(mt, a.items[1]);
      if (!r) {
        res = res_fail("no such rune: %s", a.items[1]);
      } else {
        res = res_make(1);
        char *s = cJSON_Print(r);
        res_line(res, "%s", s ? s : "{}");
        free(s);
        res_set_data(res, cJSON_Duplicate(r, 1));
      }
    }

  } else if (!strcmp(v, "find")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_fail("usage: find <query>");
    } else {
      const char *q = a.items[1];
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, vc_mantle_runes(mt)) {
        int hit = ci_contains(vc_rune_name(r), q);
        cJSON *it = NULL;
        if (!hit)
          cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(r, "tags"))
            if (cJSON_IsString(it) && ci_contains(it->valuestring, q)) { hit = 1; break; }
        if (!hit)
          cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(r, "facets"))
            if (cJSON_IsString(it) && ci_contains(it->valuestring, q)) { hit = 1; break; }
        if (!hit) {
          char *s = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(r, "content"));
          if (s && ci_contains(s, q)) hit = 1;
          free(s);
        }
        if (hit) {
          const char *nm = vc_rune_name(r);
          res_line(res, "%s", nm);
          cJSON_AddItemToArray(arr, cJSON_CreateString(nm));
        }
      }
      if (cJSON_GetArraySize(arr) == 0) res_line(res, "(no matches)");
      res_set_data(res, arr);
    }

  } else if (!strcmp(v, "axes")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count >= 2 && !strcmp(a.items[1], "all")) {
      const char *axesList = "where what who when state free";
      res = res_make(1);
      res_line(res, "axes: %s", axesList);
      res_set_data(res, cJSON_CreateString(axesList));
    } else {
      static const char *kAxes[6] = {"where", "what", "who",
                                     "when",  "state", "free"};
      cJSON *buckets = cJSON_CreateObject();
      for (int i = 0; i < 6; i++)
        cJSON_AddItemToObject(buckets, kAxes[i], cJSON_CreateArray());
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, vc_mantle_runes(mt)) {
        cJSON *it = NULL;
        cJSON_ArrayForEach(it, cJSON_GetObjectItemCaseSensitive(r, "tags")) {
          if (!cJSON_IsString(it)) continue;
          const char *tag = it->valuestring;
          cJSON *bucket = cJSON_GetObjectItemCaseSensitive(buckets, vc_axis_of(tag));
          int present = 0;
          cJSON *x = NULL;
          cJSON_ArrayForEach(x, bucket)
            if (!strcmp(x->valuestring, tag)) { present = 1; break; }
          if (!present) cJSON_AddItemToArray(bucket, cJSON_CreateString(tag));
        }
      }
      res = res_make(1);
      int any = 0;
      for (int i = 0; i < 6; i++) {
        cJSON *bucket = cJSON_GetObjectItemCaseSensitive(buckets, kAxes[i]);
        if (cJSON_GetArraySize(bucket) == 0) continue;
        char line[512];
        line[0] = 0;
        int first = 1;
        cJSON *x = NULL;
        cJSON_ArrayForEach(x, bucket) {
          if (!first) strncat(line, ", ", sizeof line - strlen(line) - 1);
          strncat(line, x->valuestring, sizeof line - strlen(line) - 1);
          first = 0;
        }
        res_line(res, "%-6s %s", kAxes[i], line);
        any = 1;
      }
      if (!any) res_line(res, "(no tags)");
      res_set_data(res, buckets);
    }

  } else if (!strcmp(v, "tree")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else {
      res = res_make(1);
      res_line(res, "%s", vc_mantle_name(mt));
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, vc_mantle_runes(mt))
        res_line(res, "  - %s [%s]", vc_rune_name(r), gstr(r, "glyph"));
      cJSON *layout = cJSON_GetObjectItemCaseSensitive(mt, "layout");
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(layout, "edges"))
        res_line(res, "  edge %s -%s-> %s", gstr(e, "from"), gstr(e, "relation"),
                 gstr(e, "to"));
    }

  } else if (!strcmp(v, "export")) {
    res = res_make(1);
    char *s = cJSON_PrintUnformatted(state);
    res_line(res, "%s", s ? s : "{}");
    free(s);
    res_set_data(res, cJSON_Duplicate(state, 1));

  }
  return res;
}
