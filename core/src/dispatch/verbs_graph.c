/* verbs_graph.c — the "graph" verb family for the dispatcher.
 * Handlers are grouped by family (SPEC §7); the router in dispatch.c tries each
 * family in turn. Bodies are unchanged from the original dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *vc_verbs_graph(VC_Manager *m, cJSON *state, vc_argv a, const char *v) {
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)m; (void)state; (void)a; (void)v; (void)err;
  if (0) {
  } else if (!strcmp(v, "link")) {
    /* link <from> <to> [--relation r] [--weight w] [--undirected] — first-class
     * link (SPEC §3.4). The substrate is permissive: links MAY dangle. */
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 3) {
      res = res_fail("usage: link <from> <to> [--relation r] [--weight w] [--undirected]");
    } else {
      const char *rel = "";
      double weight = 1.0;
      int directed = 1;
      for (int i = 3; i < a.count; i++) {
        if (!strcmp(a.items[i], "--relation") && i + 1 < a.count) rel = a.items[++i];
        else if (!strcmp(a.items[i], "--weight") && i + 1 < a.count) weight = atof(a.items[++i]);
        else if (!strcmp(a.items[i], "--undirected")) directed = 0;
      }
      /* canonicalize to rune names when they exist; otherwise keep the raw ref
       * (a dangling link to not-yet-created knowledge). */
      cJSON *rf = vc_mantle_find_rune(mt, a.items[1]);
      cJSON *rt = vc_mantle_find_rune(mt, a.items[2]);
      const char *fn = rf ? vc_rune_name(rf) : a.items[1];
      const char *tn = rt ? vc_rune_name(rt) : a.items[2];
      vc_mantle_add_edge(mt, fn, tn, rel, weight, directed);
      res = res_make(1);
      res_line(res, "link %s -%s-> %s (w=%g%s)", fn, rel, tn, weight,
               directed ? "" : ", undirected");
    }

  } else if (!strcmp(v, "unlink")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 3) {
      res = res_fail("usage: unlink <from> <to> [--relation r]");
    } else {
      const char *rel = NULL;
      for (int i = 3; i < a.count; i++)
        if (!strcmp(a.items[i], "--relation") && i + 1 < a.count) rel = a.items[++i];
      cJSON *rf = vc_mantle_find_rune(mt, a.items[1]);
      cJSON *rt = vc_mantle_find_rune(mt, a.items[2]);
      const char *fn = rf ? vc_rune_name(rf) : a.items[1];
      const char *tn = rt ? vc_rune_name(rt) : a.items[2];
      int n = vc_mantle_remove_edge(mt, fn, tn, rel);
      res = res_make(n > 0);
      if (n > 0) res_line(res, "unlinked %d link(s): %s -> %s", n, fn, tn);
      else res_line(res, "no link: %s -> %s", fn, tn);
    }

  } else if (!strcmp(v, "links")) {
    /* links [<ref>] — list links, optionally only those touching <ref>. Read-only. */
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else {
      const char *ref = (a.count >= 2 && a.items[1][0] != '-') ? a.items[1] : NULL;
      cJSON *layout = cJSON_GetObjectItemCaseSensitive(mt, "layout");
      cJSON *edges = layout ? cJSON_GetObjectItemCaseSensitive(layout, "edges") : NULL;
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, edges) {
        const char *f = gstr(e, "from"), *t = gstr(e, "to");
        if (ref && strcmp(f, ref) && strcmp(t, ref)) continue;
        cJSON *wj = cJSON_GetObjectItemCaseSensitive(e, "weight");
        double wt = cJSON_IsNumber(wj) ? wj->valuedouble : 1.0;
        cJSON *dj = cJSON_GetObjectItemCaseSensitive(e, "directed");
        int dir = cJSON_IsBool(dj) ? cJSON_IsTrue(dj) : 1;
        res_line(res, "%s -%s-> %s (w=%g%s)", f, gstr(e, "relation"), t, wt,
                 dir ? "" : ", undirected");
        cJSON_AddItemToArray(arr, cJSON_Duplicate(e, 1));
      }
      res_set_data(res, arr);
    }

  } else if (!strcmp(v, "relate")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 3) {
      res = res_fail("usage: relate <tagA> <tagB> [weight]");
    } else {
      double w = a.count >= 4 ? atof(a.items[3]) : 1.0;
      cJSON *tags = cJSON_GetObjectItemCaseSensitive(mt, "tags");
      set_near(tags, a.items[1], a.items[2], w);
      set_near(tags, a.items[2], a.items[1], w);
      res = res_make(1);
      res_line(res, "%s ~ %s (%.3g)", a.items[1], a.items[2], w);
    }

  } else if (!strcmp(v, "unrelate")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 3) {
      res = res_fail("usage: unrelate <tagA> <tagB>");
    } else {
      cJSON *tags = cJSON_GetObjectItemCaseSensitive(mt, "tags");
      del_near(tags, a.items[1], a.items[2]);
      del_near(tags, a.items[2], a.items[1]);
      res = res_make(1);
      res_line(res, "%s ~/~ %s", a.items[1], a.items[2]);
    }

  } else if (!strcmp(v, "related")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_fail("usage: related <tag>");
    } else {
      cJSON *tags = cJSON_GetObjectItemCaseSensitive(mt, "tags");
      cJSON *ta = cJSON_GetObjectItemCaseSensitive(tags, a.items[1]);
      cJSON *near = ta ? cJSON_GetObjectItemCaseSensitive(ta, "near") : NULL;
      res = res_make(1);
      cJSON *out = cJSON_CreateObject();
      if (near) {
        cJSON *e = NULL;
        cJSON_ArrayForEach(e, near) {
          res_line(res, "%s ~ %s (%.3g)", a.items[1], e->string, e->valuedouble);
          cJSON_AddNumberToObject(out, e->string, e->valuedouble);
        }
      }
      if (cJSON_GetArraySize(out) == 0) res_line(res, "(no neighbors)");
      res_set_data(res, out);
    }

  } else if (!strcmp(v, "rule")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count >= 3 && !strcmp(a.items[1], "add")) {
      cJSON *rule = cJSON_Parse(a.items[2]);
      if (!rule || !cJSON_IsObject(rule)) {
        if (rule) cJSON_Delete(rule);
        res = res_fail("rule add expects a JSON object");
      } else {
        cJSON *rules = cJSON_GetObjectItemCaseSensitive(mt, "rules");
        cJSON_AddItemToArray(rules, rule);
        res = res_make(1);
        res_line(res, "rule added (#%d) — stored, not executed (reducer reserved)",
                 cJSON_GetArraySize(rules) - 1);
      }
    } else if (a.count >= 2 && !strcmp(a.items[1], "ls")) {
      cJSON *rules = cJSON_GetObjectItemCaseSensitive(mt, "rules");
      res = res_make(1);
      int i = 0;
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, rules) {
        char *s = cJSON_PrintUnformatted(r);
        res_line(res, "#%d %s", i++, s ? s : "{}");
        free(s);
      }
      if (cJSON_GetArraySize(rules) == 0) res_line(res, "(no rules)");
      res_set_data(res, cJSON_Duplicate(rules, 1));
    } else if (a.count >= 3 && !strcmp(a.items[1], "rm")) {
      cJSON *rules = cJSON_GetObjectItemCaseSensitive(mt, "rules");
      int idx = atoi(a.items[2]);
      if (idx < 0 || idx >= cJSON_GetArraySize(rules)) {
        res = res_fail("no rule #%d", idx);
      } else {
        cJSON_DeleteItemFromArray(rules, idx);
        res = res_make(1);
        res_line(res, "removed rule #%d", idx);
      }
    } else if (a.count >= 2 && !strcmp(a.items[1], "clear")) {
      cJSON_ReplaceItemInObjectCaseSensitive(mt, "rules", cJSON_CreateArray());
      res = res_make(1);
      res_line(res, "cleared rules");
    } else {
      res = res_fail("usage: rule add '<json>' | rule ls | rule rm <i> | rule clear");
    }

  } else if (!strcmp(v, "bind")) {
    if (a.count < 5) {
      res = res_fail("usage: bind <from> <on> <to> <do> [--name N] [--note ...]");
    } else {
      cJSON *am = vc_active_mantle(state);
      const char *def = am ? vc_mantle_name(am) : NULL;
      cJSON *fm, *fr, *tm, *tr;
      if (!vc_parse_ref(state, a.items[1], def, &fm, &fr)) {
        res = res_fail("bad 'from' ref: %s", a.items[1]);
      } else if (!vc_parse_ref(state, a.items[3], def, &tm, &tr)) {
        res = res_fail("bad 'to' ref: %s", a.items[3]);
      } else {
        const char *name = NULL, *note = "";
        for (int i = 5; i < a.count; i++) {
          if (!strcmp(a.items[i], "--name") && i + 1 < a.count) name = a.items[++i];
          else if (!strcmp(a.items[i], "--note") && i + 1 < a.count) note = a.items[++i];
        }
        cJSON *from = cJSON_CreateObject();
        cJSON_AddStringToObject(from, "mantle", vc_mantle_name(fm));
        cJSON_AddStringToObject(from, "rune", vc_rune_name(fr));
        cJSON_AddStringToObject(from, "on", a.items[2]);
        cJSON *to = cJSON_CreateObject();
        cJSON_AddStringToObject(to, "mantle", vc_mantle_name(tm));
        cJSON_AddStringToObject(to, "rune", vc_rune_name(tr));
        cJSON_AddStringToObject(to, "do", a.items[4]);
        cJSON *b = vc_binding_new(name, from, to, note);
        cJSON_AddItemToArray(vc_bindings(state), b);
        res = res_make(1);
        res_line(res, "bound %s:%s --%s--> %s:%s (%s)", vc_mantle_name(fm),
                 vc_rune_name(fr), a.items[2], vc_mantle_name(tm),
                 vc_rune_name(tr), a.items[4]);
        res_set_data(res, cJSON_CreateString(gstr(b, "id")));
      }
    }

  } else if (!strcmp(v, "bindings")) {
    const char *rune_filter =
        (a.count >= 2 && a.items[1][0] != '-') ? a.items[1] : NULL;
    res = res_make(1);
    cJSON *arr = cJSON_CreateArray();
    cJSON *b = NULL;
    cJSON_ArrayForEach(b, vc_bindings(state)) {
      cJSON *from = cJSON_GetObjectItemCaseSensitive(b, "from");
      cJSON *to = cJSON_GetObjectItemCaseSensitive(b, "to");
      if (rune_filter && strcmp(gstr(from, "rune"), rune_filter) &&
          strcmp(gstr(to, "rune"), rune_filter))
        continue;
      res_line(res, "%s  %s:%s --%s--> %s:%s", gstr(b, "id"),
               gstr(from, "mantle"), gstr(from, "rune"), gstr(from, "on"),
               gstr(to, "mantle"), gstr(to, "rune"));
      cJSON_AddItemToArray(arr, cJSON_Duplicate(b, 1));
    }
    if (cJSON_GetArraySize(arr) == 0) res_line(res, "(no bindings)");
    res_set_data(res, arr);

  } else if (!strcmp(v, "unbind")) {
    if (a.count < 2) {
      res = res_fail("usage: unbind <id|name>");
    } else {
      cJSON *binds = vc_bindings(state);
      int idx = 0, removed = 0;
      cJSON *b = NULL;
      cJSON_ArrayForEach(b, binds) {
        cJSON *nm = cJSON_GetObjectItemCaseSensitive(b, "name");
        if (!strcmp(gstr(b, "id"), a.items[1]) ||
            (cJSON_IsString(nm) && !strcmp(nm->valuestring, a.items[1]))) {
          cJSON_DeleteItemFromArray(binds, idx);
          removed = 1;
          break;
        }
        idx++;
      }
      res = removed ? res_make(1) : res_fail("no such binding: %s", a.items[1]);
      if (removed) res_line(res, "unbound %s", a.items[1]);
    }

  }
  return res;
}
