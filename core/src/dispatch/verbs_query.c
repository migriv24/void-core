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
    /* HAND-MAINTAINED, and therefore drift-prone: Void Hormiga found `link`
     * missing here on 2026-09-02, and the same hole had swallowed `links`,
     * `unlink` and `journal`. The rest of the briefing is introspected from live
     * registries; this list is not, because the router is an if/else chain with
     * nothing to enumerate. `describe_verbs_test.py` now diffs this string
     * against every verb the families actually answer to, so the next omission
     * fails a test instead of costing an agent an hour. Keep them in sync. */
    const char *verbs =
        "version help glyphs glyph mantles mantle use where rune ls find describe get "
        "set setjson tag facet place measure axes cat tree validate export undo redo history "
        "journal status diff revert save build deploy preview effect log config "
        "bind bindings unbind batch link unlink links values relate unrelate related rule script";
    res = res_make(1);
    res_line(res, "verbs: %s", verbs);
    res_line(res, "posix aliases: cd pwd rm mv cp mkdir rmdir grep man ? quit dump");
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
    cJSON *d = cJSON_CreateObject(); /* structured answer, not lines-only (VLS) */
    if (mt) cJSON_AddStringToObject(d, "mantle", vc_mantle_name(mt));
    else cJSON_AddNullToObject(d, "mantle");
    if (cJSON_IsString(dom)) cJSON_AddStringToObject(d, "domain", dom->valuestring);
    else cJSON_AddNullToObject(d, "domain");
    res_set_data(res, d);

  } else if (!strcmp(v, "glyphs")) {
    /* glyphs [<name>] [--kind entity|act|measure] — SPEC §3.3.3.
     *
     * THE DESCRIPTOR IS A HOST CONTRACT. What `data` carries here is the object
     * a host may read `fields`, `kind`, `kinds` and `presentations` from — with
     * the defaults resolved, so one shape answers whatever the author wrote —
     * and it is the reason no host should keep a second, hand-maintained copy of
     * its own schema. (Void Hormiga kept one, `glyph_fields()`, whose comment
     * read "the one place the schema is written twice, until a core verb exposes
     * glyph descriptors to hosts." This is that verb, said out loud.)
     *
     * Two registries answer: DECLARED descriptors in `state.glyphs` (they
     * travel with the document) shadow REGISTERED ones on the manager (host
     * config). `source` on each descriptor says which, so the shadowing is never
     * silent. */
    const char *want = NULL, *kind_filter = NULL;
    for (int i = 1; i < a.count; i++) {
      if (!strcmp(a.items[i], "--kind") && i + 1 < a.count) kind_filter = a.items[++i];
      else if (a.items[i][0] != '-' && !want) want = a.items[i];
    }
    cJSON *declared = cJSON_GetObjectItemCaseSensitive(state, "glyphs");
    if (want) {
      cJSON *gd = vc_glyph_lookup(m, want);
      if (!gd) {
        res = res_fail("unknown glyph: %s (try 'glyphs')", want);
      } else {
        cJSON *full = vc_glyph_resolved(m, gd, want);
        res = res_make(1);
        char *s = cJSON_Print(full);
        res_line(res, "%s", s ? s : "{}");
        free(s);
        res_set_data(res, full);
      }
    } else {
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      for (int pass = 0; pass < 2; pass++) {
        cJSON *src = pass == 0 ? declared : m->glyphs;
        cJSON *gd = NULL;
        cJSON_ArrayForEach(gd, src) {
          const char *nm = gd->string ? gd->string : gstr(gd, "glyph");
          if (pass == 1 && vc_glyph_find(declared, nm)) continue; /* shadowed */
          if (kind_filter && strcmp(vc_glyph_kind(gd), kind_filter)) continue;
          res_line(res, "%-12s %-7s %-8s %s", nm, vc_glyph_kind(gd),
                   pass == 0 ? "document" : "host", gstr(gd, "label"));
          cJSON_AddItemToArray(arr, vc_glyph_resolved(m, gd, nm));
        }
      }
      if (cJSON_GetArraySize(arr) == 0) res_line(res, "(no matching glyphs)");
      res_set_data(res, arr);
    }

  } else if (!strcmp(v, "ls")) {
    cJSON *mt = vc_active_mantle(state);
    if (!mt) {
      /* root-ls (SPEC §7): with no active mantle, list the mantles — a cold
       * start is self-explanatory instead of an error. data = mantle names. */
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      cJSON *mm = NULL;
      cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles")) {
        const char *nm = vc_mantle_name(mm);
        res_line(res, "  %s/", nm);
        cJSON_AddItemToArray(arr, cJSON_CreateString(nm));
      }
      if (cJSON_GetArraySize(arr) == 0)
        res_line(res, "(no mantles — create one with 'mantle new <name>')");
      res_set_data(res, arr);
    } else {
      /* optional "--tag <expr>": join the tokens after the flag so an unquoted
       * multi-word expression still works — but stop at the next `--flag`, so a
       * trailing flag (e.g. `--json` appended by a $(…) capture) never joins
       * into the tag expression. */
      char expr[1024];
      expr[0] = 0;
      int have_expr = 0;
      /* `--kind entity|act|measure` (SPEC §3.3.1) filters by the rune's KIND,
       * which lives on its glyph rather than in its tags — so it is a separate
       * flag rather than a reserved `kind:` tag. `kind:` is already an ordinary
       * app namespace on the `what` axis (§5), and reserving it would silently
       * change what every existing `kind:vegetable` tag matches. */
      const char *kind_filter = NULL;
      for (int i = 1; i < a.count; i++) {
        if (!strcmp(a.items[i], "--kind") && i + 1 < a.count) {
          kind_filter = a.items[++i];
        } else if (!strcmp(a.items[i], "--tag")) {
          have_expr = 1;
          int j = i + 1;
          for (; j < a.count; j++) {
            if (!strncmp(a.items[j], "--", 2)) break;
            if (j > i + 1) strncat(expr, " ", sizeof expr - strlen(expr) - 1);
            strncat(expr, a.items[j], sizeof expr - strlen(expr) - 1);
          }
          /* Resume at the flag that ENDED the expression rather than stopping —
           * `ls --tag june --kind measure` must see both. The expression itself
           * still runs only to the next `--flag` (the 2026-07-03 capture-flag
           * fix, conformance case 08). */
          i = j - 1;
        }
      }
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, vc_mantle_runes(mt)) {
        if (have_expr && !vc_filter_eval(r, expr)) continue;
        if (kind_filter) {
          cJSON *gd = vc_glyph_lookup(m, gstr(r, "glyph"));
          if (!gd || strcmp(vc_glyph_kind(gd), kind_filter)) continue;
        }
        const char *nm = vc_rune_name(r);
        res_line(res, "%s", nm);
        cJSON_AddItemToArray(arr, cJSON_CreateString(nm));
      }
      if (cJSON_GetArraySize(arr) == 0)
        res_line(res, (have_expr || kind_filter) ? "(no matches)" : "(empty)");
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
        cJSON *gd = vc_glyph_lookup(m, gstr(r, "glyph"));
        res_line(res, "%s  [glyph %s / %s]  id=%s", vc_rune_name(r), gstr(r, "glyph"),
                 gd ? vc_glyph_kind(gd) : "unregistered", vc_rune_id(r));
        cJSON *q = cJSON_GetObjectItemCaseSensitive(r, "quantity");
        if (cJSON_IsObject(q)) {
          const char *unit = gstr(q, "unit"), *level = gstr(q, "level");
          res_line(res, "  measures %s%s%s", *unit ? unit : "(no unit)",
                   *level ? ", level " : "", *level ? level : "");
        }
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
        /* A trailing `--flag` is not a field name. `$(get <ref> --json)` — the
         * natural way to capture a rune's whole content — used to look up a
         * content field literally called "--json" and fail; the same shape as
         * the 2026-07-03 capture-flag bug in `ls --tag` (conformance case 08). */
        if (a.count >= 3 && strncmp(a.items[2], "--", 2)) {
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
