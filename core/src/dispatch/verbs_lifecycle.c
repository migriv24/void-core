/* verbs_lifecycle.c — the "lifecycle" verb family for the dispatcher.
 * Handlers are grouped by family (SPEC §7); the router in dispatch.c tries each
 * family in turn. Bodies are unchanged from the original dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *vc_verbs_lifecycle(VC_Manager *m, cJSON *state, vc_argv a, const char *v) {
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)m; (void)state; (void)a; (void)v; (void)err;
  if (0) {
  } else if (!strcmp(v, "undo")) {
    int n = a.count >= 2 ? atoi(a.items[1]) : 1;
    if (n < 1) n = 1;
    int done = vc_undo(m, n);
    if (done == 0) {
      /* Distinguish "no frames" from "no frames ever" — a host that called
       * vc_set_undo(m, 0) struck that bargain knowingly, but a script running
       * inside it did not, and "nothing to undo" would read as a state fact. */
      res = m->undo_on ? res_fail("nothing to undo")
                       : res_fail("undo is disabled on this manager "
                                  "(vc_set_undo); nothing is being recorded");
    } else {
      res = res_make(1);
      res_line(res, "undid %d change(s)", done);
      vc_log(m, "INFO", "undo", "undid %d change(s)", done);
    }

  } else if (!strcmp(v, "redo")) {
    int n = a.count >= 2 ? atoi(a.items[1]) : 1;
    if (n < 1) n = 1;
    int done = vc_redo(m, n);
    if (done == 0) {
      res = m->undo_on ? res_fail("nothing to redo")
                       : res_fail("undo is disabled on this manager "
                                  "(vc_set_undo); nothing is being recorded");
    } else {
      res = res_make(1);
      res_line(res, "redid %d change(s)", done);
      vc_log(m, "INFO", "redo", "redid %d change(s)", done);
    }

  } else if (!strcmp(v, "history")) {
    int tail = -1;
    for (int i = 1; i < a.count; i++) {
      if (!strcmp(a.items[i], "--tail") && i + 1 < a.count)
        tail = atoi(a.items[++i]);
    }
    cJSON *labels = vc_history(m);
    int total = cJSON_GetArraySize(labels);
    int start = (tail > 0 && tail < total) ? total - tail : 0;
    res = res_make(1);
    for (int i = start; i < total; i++) {
      cJSON *it = cJSON_GetArrayItem(labels, i);
      /* attribution suffix (SPEC §9); data stays a plain label array */
      const char *w = (i < m->undo_count) ? m->undo[i].who : NULL;
      if (w)
        res_line(res, "%d  %s  [%s]", i + 1,
                 cJSON_IsString(it) ? it->valuestring : "", w);
      else
        res_line(res, "%d  %s", i + 1, cJSON_IsString(it) ? it->valuestring : "");
    }
    if (total == 0)
      res_line(res, m->undo_on ? "(no history)"
                               : "(undo is disabled on this manager)");
    res_set_data(res, labels);

  } else if (!strcmp(v, "validate")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else {
      int quiet = 0;
      for (int i = 1; i < a.count; i++)
        if (!strcmp(a.items[i], "--quiet")) quiet = 1;
      cJSON *problems = cJSON_CreateArray();
      cJSON *runes = vc_mantle_runes(mt);
      char b[256];
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, runes) {
        const char *nm = vc_rune_name(r);
        cJSON *r2 = NULL;
        cJSON_ArrayForEach(r2, runes) {
          if (r2 == r) break;
          if (!strcmp(vc_rune_name(r2), nm)) {
            snprintf(b, sizeof b, "duplicate name: %s", nm);
            cJSON_AddItemToArray(problems, cJSON_CreateString(b));
            break;
          }
        }
        const char *g = gstr(r, "glyph");
        if (!vc_glyph_find(m->glyphs, g)) {
          snprintf(b, sizeof b, "unregistered glyph '%s' on %s", g, nm);
          cJSON_AddItemToArray(problems, cJSON_CreateString(b));
        }
      }
      cJSON *layout = cJSON_GetObjectItemCaseSensitive(mt, "layout");
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, cJSON_GetObjectItemCaseSensitive(layout, "edges")) {
        const char *ends[2] = {gstr(e, "from"), gstr(e, "to")};
        const char *side[2] = {"from", "to"};
        for (int k = 0; k < 2; k++) {
          if (vc_mantle_find_rune(mt, ends[k])) continue;   /* resolves here: fine */
          /* SPEC §3.7 — an unresolved endpoint has TWO causes, and they used to
           * read identically. A DANGLE is legitimate: links tolerate
           * not-yet-created knowledge, and a host that streams chunks in and out
           * has edges dangle constantly with nothing wrong. A CROSS-KIND endpoint
           * is a mistake: the name resolves, but to a MANTLE, and v1 links are
           * rune↔rune inside one mantle. Collapsing them told a host its typo was
           * the case it had been told to ignore (Void Unity, 2026-08-28, boxing an
           * equipment mantle into a world mantle). Mantle names are unique in the
           * state document, so the test is exact rather than a guess. */
          if (vc_state_find_mantle(state, ends[k]))
            snprintf(b, sizeof b,
                     "cross-kind edge %s '%s': names a mantle, not a rune",
                     side[k], ends[k]);
          else
            snprintf(b, sizeof b, "dangling edge %s '%s'", side[k], ends[k]);
          cJSON_AddItemToArray(problems, cJSON_CreateString(b));
        }
      }
      int n = cJSON_GetArraySize(problems);
      res = res_make(n == 0);
      if (!quiet) {
        if (n == 0) res_line(res, "valid");
        else {
          cJSON *p = NULL;
          cJSON_ArrayForEach(p, problems) res_line(res, "%s", p->valuestring);
        }
      }
      res_set_data(res, problems);
    }

  } else if (!strcmp(v, "status")) {
    int dirtyflag = 0;
    for (int i = 1; i < a.count; i++)
      if (!strcmp(a.items[i], "--dirty")) dirtyflag = 1;
    cJSON *d = vc_compute_diff(state);
    int na = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(d, "added"));
    int nr = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(d, "removed"));
    int nc = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(d, "changed"));
    int dirty = na || nr || nc;
    if (dirtyflag) {
      res = res_make(dirty);
      res_line(res, dirty ? "dirty" : "clean");
      cJSON_Delete(d);
    } else {
      res = res_make(1);
      res_line(res, "added %d, changed %d, removed %d", na, nc, nr);
      cJSON *x = NULL;
      cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(d, "added"))
        res_line(res, "  + %s", x->valuestring);
      cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(d, "changed"))
        res_line(res, "  ~ %s", x->valuestring);
      cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(d, "removed"))
        res_line(res, "  - %s", x->valuestring);
      if (!dirty) res_line(res, "(clean)");
      res_set_data(res, d);
    }

  } else if (!strcmp(v, "diff")) {
    cJSON *d = vc_compute_diff(state);
    res = res_make(1);
    cJSON *x = NULL;
    cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(d, "added"))
      res_line(res, "+ %s", x->valuestring);
    cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(d, "changed"))
      res_line(res, "~ %s", x->valuestring);
    cJSON_ArrayForEach(x, cJSON_GetObjectItemCaseSensitive(d, "removed"))
      res_line(res, "- %s", x->valuestring);
    res_set_data(res, d);

  } else if (!strcmp(v, "revert")) {
    cJSON *base = cJSON_GetObjectItemCaseSensitive(state, "_baseline");
    cJSON_ReplaceItemInObjectCaseSensitive(state, "mantles",
                                           cJSON_Duplicate(base, 1));
    vc_log(m, "INFO", "revert", "working changes discarded");
    res = res_make(1);
    res_line(res, "reverted to last save");

  } else if (!strcmp(v, "save")) {
    res = res_make(1);
    if (m->effect) {
      char *st = cJSON_PrintUnformatted(state);
      m->effect_fired = 1; /* observed holiday crossing (SPEC §6.2) */
      char *hr = m->effect("save", st, m->effect_user);
      free(st);
      if (hr) {
        res_line(res, "adapter: %s", hr);
        free(hr);
      }
    } else {
      res_line(res, "(no host adapter; model-side save only)");
    }
    vc_snapshot_baseline(state);
    vc_log(m, "INFO", "save", "saved; baseline updated");
    res_line(res, "saved (baseline updated)");

  } else if (!strcmp(v, "deploy") || !strcmp(v, "build") ||
             !strcmp(v, "preview")) {
    cJSON *payload = cJSON_CreateObject();
    cJSON *args = cJSON_AddArrayToObject(payload, "args");
    for (int i = 1; i < a.count; i++)
      cJSON_AddItemToArray(args, cJSON_CreateString(a.items[i]));
    char *pj = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (m->effect) {
      m->effect_fired = 1; /* observed holiday crossing (SPEC §6.2) */
      char *hr = m->effect(v, pj, m->effect_user);
      res = res_make(1);
      vc_log(m, "INFO", v, "host effect invoked");
      if (hr) {
        res_line(res, "%s", hr);
        cJSON *d = cJSON_Parse(hr);
        res_set_data(res, d ? d : cJSON_CreateString(hr));
        free(hr);
      } else {
        res_line(res, "%s: done", v);
      }
    } else {
      res = res_fail("no host effect handler for '%s' "
                     "(register one via vc_set_effect_handler)", v);
      vc_log(m, "WARN", v, "no host effect handler");
    }
    free(pj);

  } else if (!strcmp(v, "log")) {
    int tail = -1;
    const char *lvl = NULL;
    for (int i = 1; i < a.count; i++) {
      if (!strcmp(a.items[i], "--tail") && i + 1 < a.count)
        tail = atoi(a.items[++i]);
      else if (!strcmp(a.items[i], "--level") && i + 1 < a.count)
        lvl = a.items[++i];
    }
    cJSON *records = cJSON_CreateArray();
    cJSON *rec = NULL;
    cJSON_ArrayForEach(rec, vc_log_buffer(m)) {
      if (lvl && strcmp(gstr(rec, "level"), lvl)) continue;
      cJSON_AddItemToArray(records, cJSON_Duplicate(rec, 1));
    }
    int total = cJSON_GetArraySize(records);
    int start = (tail > 0 && tail < total) ? total - tail : 0;
    res = res_make(1);
    for (int i = start; i < total; i++) {
      cJSON *r = cJSON_GetArrayItem(records, i);
      const char *w = gstr(r, "who");
      if (w && *w)
        res_line(res, "[%s] %s %s (%s): %s", gstr(r, "ts"), gstr(r, "level"),
                 gstr(r, "op"), w, gstr(r, "msg"));
      else
        res_line(res, "[%s] %s %s: %s", gstr(r, "ts"), gstr(r, "level"),
                 gstr(r, "op"), gstr(r, "msg"));
    }
    if (total == 0) res_line(res, "(log empty)");
    res_set_data(res, records);

  } else if (!strcmp(v, "journal")) {
    /* The §6.2 record, on the verb surface rather than only on the ABI: the core
     * is agent-drivable through verbs, and a record an agent cannot read is a
     * record it cannot reason about. `journal` itself is neither mutating nor
     * effectful, so reading or toggling the record never appears in it. */
    /* Skip flags when looking for the subcommand: capture appends `--json`
     * (§8.1), so `journal --json` is a read, not a malformed subcommand. */
    const char *sub = "";
    for (int i = 1; i < a.count; i++) {
      if (a.items[i][0] == '-' && a.items[i][1] == '-') continue;
      sub = a.items[i];
      break;
    }
    if (!strcmp(sub, "on") || !strcmp(sub, "off")) {
      m->journal_on = !strcmp(sub, "on");
      res = res_make(1);
      res_line(res, "journal %s", sub);
    } else if (!strcmp(sub, "clear")) {
      vc_journal_clear_all(m);
      res = res_make(1);
      res_line(res, "journal cleared");
    } else if (!*sub) {
      cJSON *entries = vc_journal_json(m);
      res = res_make(1);
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, entries) {
        const char *w = gstr(e, "who");
        res_line(res, "%d %s %s%s%s",
                 (int)cJSON_GetObjectItemCaseSensitive(e, "seq")->valuedouble,
                 cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(e, "pure"))
                     ? "pure" : "effectful",
                 gstr(e, "command"), (w && *w) ? " " : "", (w && *w) ? w : "");
      }
      if (cJSON_GetArraySize(entries) == 0)
        res_line(res, m->journal_on ? "(journal empty)" : "(journal off)");
      res_set_data(res, entries);
    } else {
      res = res_fail("usage: journal [on|off|clear]");
    }

  } else if (!strcmp(v, "effect")) {
    /* Generic host-effect call (the holiday boundary, beyond save/deploy/build/
     * preview): `effect <op> [args...]` invokes the registered handler with op +
     * {"args":[...]} and returns its parsed result as data. Read-only to the core
     * (the host decides what the effect does), so it is not undo-tracked. */
    if (a.count < 2) {
      res = res_fail("usage: effect <op> [args...]");
    } else if (!m->effect) {
      res = res_fail("no host effect handler for 'effect' "
                     "(register one via vc_set_effect_handler)");
      vc_log(m, "WARN", "effect", "no host effect handler");
    } else {
      const char *op = a.items[1];
      cJSON *payload = cJSON_CreateObject();
      cJSON *args = cJSON_AddArrayToObject(payload, "args");
      for (int i = 2; i < a.count; i++)
        cJSON_AddItemToArray(args, cJSON_CreateString(a.items[i]));
      char *pj = cJSON_PrintUnformatted(payload);
      cJSON_Delete(payload);
      m->effect_fired = 1; /* observed holiday crossing (SPEC §6.2) */
      char *hr = m->effect(op, pj, m->effect_user);
      free(pj);
      res = res_make(1);
      vc_log(m, "INFO", "effect", op);
      if (hr) {
        cJSON *d = cJSON_Parse(hr);
        res_set_data(res, d ? d : cJSON_CreateString(hr));
        res_line(res, "effect %s: ok", op);
        free(hr);
      } else {
        res_line(res, "effect %s: done", op);
      }
    }

  }
  return res;
}
