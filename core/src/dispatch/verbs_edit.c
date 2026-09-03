/* verbs_edit.c — the "edit" verb family for the dispatcher.
 * Handlers are grouped by family (SPEC §7); the router in dispatch.c tries each
 * family in turn. Bodies are unchanged from the original dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Scalar coercion for `config set`, mirroring the JS oracle's coerce():
 * "true"/"false" -> bool; a token that starts -?digit and parses fully as a
 * number -> number; anything else stays a string. */
static cJSON *coerce_scalar(const char *s) {
  if (!strcmp(s, "true")) return cJSON_CreateTrue();
  if (!strcmp(s, "false")) return cJSON_CreateFalse();
  const char *q = (*s == '-') ? s + 1 : s;
  if (isdigit((unsigned char)*q)) {
    char *end = NULL;
    double d = strtod(s, &end);
    if (end && end != s && *end == 0) return cJSON_CreateNumber(d);
  }
  return cJSON_CreateString(s);
}

/* Value -> display string (malloc'd; caller frees). Strings print bare (no
 * quotes), bools as true/false, missing/null as "", numbers via cJSON. */
static char *scalar_str(const cJSON *val) {
  if (!val || cJSON_IsNull(val)) { char *z = (char *)malloc(1); if (z) *z = 0; return z; }
  if (cJSON_IsString(val)) {
    size_t n = strlen(val->valuestring) + 1;
    char *z = (char *)malloc(n);
    if (z) memcpy(z, val->valuestring, n);
    return z;
  }
  return cJSON_PrintUnformatted(val);
}

cJSON *vc_verbs_edit(VC_Manager *m, cJSON *state, vc_argv a, const char *v) {
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)m; (void)state; (void)a; (void)v; (void)err;
  if (0) {
  } else if (!strcmp(v, "config")) {
    /* SPEC §7 system family: config | config get <k> | config set <k> <v...>.
     * Host/session meta (state.config) — not part of the undoable mantle slice
     * (matches the JS oracle: no undo frame), so a live meta change (e.g. a
     * DAW's bpm) never disturbs undo history. */
    cJSON *cfg = cJSON_GetObjectItemCaseSensitive(state, "config");
    if (a.count >= 2 && !strcmp(a.items[1], "set")) {
      if (a.count < 3) {
        res = res_fail("usage: config set <key> <value>");
      } else {
        size_t len = 1;
        for (int i = 3; i < a.count; i++) len += strlen(a.items[i]) + 1;
        char *joined = (char *)malloc(len);
        joined[0] = 0;
        for (int i = 3; i < a.count; i++) {
          if (i > 3) strcat(joined, " ");
          strcat(joined, a.items[i]);
        }
        cJSON *val = coerce_scalar(joined);
        cJSON_DeleteItemFromObjectCaseSensitive(cfg, a.items[2]);
        cJSON_AddItemToObject(cfg, a.items[2], val);
        char *vs = scalar_str(val);
        res = res_make(1);
        res_line(res, "config %s = %s", a.items[2], vs ? vs : "");
        free(vs);
        free(joined);
      }
    } else if (a.count >= 2 && !strcmp(a.items[1], "get")) {
      cJSON *val = a.count >= 3
                       ? cJSON_GetObjectItemCaseSensitive(cfg, a.items[2])
                       : NULL;
      char *vs = scalar_str(val);
      res = res_make(1);
      res_line(res, "%s", vs ? vs : "");
      free(vs);
      res_set_data(res, val ? cJSON_Duplicate(val, 1) : cJSON_CreateNull());
    } else if (a.count >= 2) {
      res = res_fail("usage: config [get <key> | set <key> <value>]");
    } else {
      res = res_make(1);
      int n = 0;
      cJSON *it = NULL;
      cJSON_ArrayForEach(it, cfg) {
        char *vs = scalar_str(it);
        res_line(res, "%s = %s", it->string, vs ? vs : "");
        free(vs);
        n++;
      }
      if (n == 0) res_line(res, "(config empty)");
      res_set_data(res, cJSON_Duplicate(cfg, 1));
    }

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
    /* SPEC §3.4/§7.2 — the mantle lifecycle family, the mantle-level analogues
     * of `rune new|rm|rename`. All three mutate the undoable slice
     * (mantles + active), so they take a normal undo frame. */
    cJSON *mantles = cJSON_GetObjectItemCaseSensitive(state, "mantles");
    cJSON *active = cJSON_GetObjectItemCaseSensitive(state, "active");
    if (a.count >= 3 && !strcmp(a.items[1], "new")) {
      const char *name = a.items[2];
      int dup = 0;
      cJSON *mm = NULL;
      cJSON_ArrayForEach(mm, mantles) {
        if (!strcmp(vc_mantle_name(mm), name)) { dup = 1; break; }
      }
      if (dup) {
        res = res_fail("mantle exists: %s", name);
      } else {
        cJSON_AddItemToArray(mantles, vc_mantle_new(name, NULL));
        cJSON_ReplaceItemInObjectCaseSensitive(active, "mantle",
                                               cJSON_CreateString(name));
        res = res_make(1);
        res_line(res, "created mantle: %s (active)", name);
      }
    } else if (a.count >= 3 && !strcmp(a.items[1], "rm")) {
      /* Removing the ACTIVE mantle deactivates (the `use` / `cd /` cold-start
       * semantics) rather than refusing — the core does the obvious thing
       * instead of making the caller `use` elsewhere first. Removing the last
       * mantle is allowed: root-`ls` already handles an empty mantle list. */
      const char *name = a.items[2];
      int idx = 0, found = -1;
      cJSON *mm = NULL;
      cJSON_ArrayForEach(mm, mantles) {
        if (!strcmp(vc_mantle_name(mm), name)) { found = idx; break; }
        idx++;
      }
      if (found < 0) {
        res = res_fail("no such mantle: %s", name);
      } else {
        cJSON *am = cJSON_GetObjectItemCaseSensitive(active, "mantle");
        int was_active = cJSON_IsString(am) && !strcmp(am->valuestring, name);
        cJSON_DeleteItemFromArray(mantles, found);
        res = res_make(1);
        if (was_active) {
          cJSON_ReplaceItemInObjectCaseSensitive(active, "mantle",
                                                 cJSON_CreateNull());
          res_line(res, "removed mantle: %s (no active mantle)", name);
        } else {
          res_line(res, "removed mantle: %s", name);
        }
      }
    } else if (a.count >= 4 && !strcmp(a.items[1], "rename")) {
      const char *old = a.items[2], *nw = a.items[3];
      cJSON *src = NULL, *mm = NULL;
      int dup = 0;
      cJSON_ArrayForEach(mm, mantles) {
        const char *nm = vc_mantle_name(mm);
        if (!src && !strcmp(nm, old)) src = mm;
        if (!strcmp(nm, nw)) dup = 1;
      }
      if (!src) {
        res = res_fail("no such mantle: %s", old);
      } else if (dup) {
        res = res_fail("mantle exists: %s", nw);
      } else {
        cJSON_ReplaceItemInObjectCaseSensitive(src, "name",
                                               cJSON_CreateString(nw));
        cJSON *am = cJSON_GetObjectItemCaseSensitive(active, "mantle");
        if (cJSON_IsString(am) && !strcmp(am->valuestring, old))
          cJSON_ReplaceItemInObjectCaseSensitive(active, "mantle",
                                                 cJSON_CreateString(nw));
        res = res_make(1);
        res_line(res, "renamed mantle %s -> %s", old, nw);
      }
    } else {
      res = res_fail("usage: mantle new <name> | mantle rm <name> | "
                     "mantle rename <old> <new>");
    }

  } else if (!strcmp(v, "rune")) {
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count >= 4 && !strcmp(a.items[1], "new")) {
      /* Either registry answers (SPEC §3.3.3): a glyph DECLARED in the document
       * is as real as one the host registered — and is the only one a bundle
       * opened somewhere else still has. */
      cJSON *gd = vc_glyph_lookup(m, a.items[2]);
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

  } else if (!strcmp(v, "glyph")) {
    /* glyph declare '<json>' | glyph undeclare <name> — SPEC §3.3.3.
     *
     * The DECLARED half of the registry. Until 0.2.14 a descriptor could only be
     * REGISTERED, on the manager, by the host at boot — so a `.miga` carried its
     * runes but not their meaning: open the bundle on a machine whose host
     * registered different descriptors and the content survives verbatim in the
     * document while the projection has no fields. Data present and unreachable.
     * A declaration lives in `state.glyphs`, which makes declaring a type an
     * ordinary logged, journaled, undoable, mergeable command like every other
     * change — the property that makes the rest of this stack trustworthy.
     * (Void Hormiga, 2026-09-03: this was the one ask that blocked something.) */
    cJSON *declared = vc_glyphs_declared(state);
    if (a.count >= 3 && !strcmp(a.items[1], "declare")) {
      /* Probe first, only to name the glyph in the reply and to say whether this
       * replaced an earlier declaration. The register call re-parses and owns
       * the real one. */
      char name[128];
      name[0] = 0;
      cJSON *probe = cJSON_Parse(a.items[2]);
      cJSON *pn = probe ? cJSON_GetObjectItemCaseSensitive(probe, "glyph") : NULL;
      if (cJSON_IsString(pn)) {
        strncpy(name, pn->valuestring, sizeof name - 1);
        name[sizeof name - 1] = 0;
      }
      if (probe) cJSON_Delete(probe);
      int existed = *name && cJSON_GetObjectItemCaseSensitive(declared, name) != NULL;
      char gerr[256];
      gerr[0] = 0;
      if (!vc_glyph_register_err(declared, a.items[2], gerr, sizeof gerr)) {
        res = res_fail("glyph declare: %s", gerr);
      } else {
        cJSON *def = vc_glyph_find(declared, name);
        int nf = cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(def, "fields"));
        res = res_make(1);
        res_line(res, "%s glyph '%s' (kind %s, %d field%s) in the state document",
                 existed ? "redeclared" : "declared", name, vc_glyph_kind(def),
                 nf, nf == 1 ? "" : "s");
        if (!existed && vc_glyph_find(m->glyphs, name))
          res_line(res, "  (shadows the host registration of the same name — "
                        "the declaration is the one that travels with the data)");
        res_set_data(res, vc_glyph_resolved(m, def, name));
      }
    } else if (a.count >= 3 && !strcmp(a.items[1], "undeclare")) {
      const char *name = a.items[2];
      if (!cJSON_GetObjectItemCaseSensitive(declared, name)) {
        res = res_fail("no declared glyph: %s (host registrations are not "
                       "undeclarable — they are the host's own config)", name);
      } else {
        /* A declaration is what makes its runes readable. Removing one while
         * runes still carry it would leave exactly the failure this feature
         * exists to prevent — content present, meaning gone — so the refusal
         * names the rune that is holding it. */
        const char *user = NULL, *user_mantle = NULL;
        cJSON *mm = NULL;
        cJSON_ArrayForEach(mm, cJSON_GetObjectItemCaseSensitive(state, "mantles")) {
          cJSON *r = NULL;
          cJSON_ArrayForEach(r, vc_mantle_runes(mm)) {
            if (!strcmp(gstr(r, "glyph"), name)) {
              user = vc_rune_name(r);
              user_mantle = vc_mantle_name(mm);
              break;
            }
          }
          if (user) break;
        }
        if (user) {
          res = res_fail("glyph '%s' is in use by rune '%s' in mantle '%s' — "
                         "remove or re-glyph its runes first", name, user, user_mantle);
        } else {
          cJSON_DeleteItemFromObjectCaseSensitive(declared, name);
          res = res_make(1);
          res_line(res, "undeclared glyph '%s'", name);
          if (vc_glyph_find(m->glyphs, name))
            res_line(res, "  (the host registration of '%s' is visible again)", name);
        }
      }
    } else {
      res = res_fail("usage: glyph declare '<json descriptor>' | "
                     "glyph undeclare <name>   ('glyphs' lists them)");
    }

  } else if (!strcmp(v, "measure")) {
    /* measure <ref> [--level L] [--unit U] [--min N] [--max N] [--clear]
     *
     * The quantity a MEASURE rune names (SPEC §3.2, §3.3.2). `player -w-> speed`
     * says the player's speed is 5; what makes that 5 a value rather than a bare
     * number is the unit, and the unit belongs to the rune `speed` — not to its
     * glyph, since `health`, `speed` and `strength` share one schema and differ
     * only in what they measure.
     *
     * It sits BESIDE `content`, not in it, for the same reason `placement` does
     * (§3.2): the core must be able to read it — an attribute assertion whose
     * unit the core cannot see gives a host back the dimensional analysis it was
     * meant to get from the graph — and `content` stays opaque. */
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_fail("usage: measure <ref> [--level nominal|ordinal|interval|ratio] "
                     "[--unit U] [--min N] [--max N] [--clear]");
    } else {
      cJSON *r = vc_mantle_find_rune(mt, a.items[1]);
      cJSON *gd = r ? vc_glyph_lookup(m, gstr(r, "glyph")) : NULL;
      if (!r) {
        res = res_fail("no such rune: %s", a.items[1]);
      } else if (!gd) {
        res = res_fail("rune '%s' carries the unregistered glyph '%s' — declare it "
                       "before annotating what it measures", vc_rune_name(r),
                       gstr(r, "glyph"));
      } else if (strcmp(vc_glyph_kind(gd), "measure")) {
        res = res_fail("'%s' is a %s rune, not a measure — a quantity annotation "
                       "belongs on the dimension, not on the thing that has an "
                       "amount of it (declare its glyph with \"kind\":\"measure\")",
                       vc_rune_name(r), vc_glyph_kind(gd));
      } else {
        int clear = 0, writes = 0;
        cJSON *q = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(r, "quantity"), 1);
        if (!cJSON_IsObject(q)) {
          if (q) cJSON_Delete(q);
          q = cJSON_CreateObject();
        }
        char qerr[256];
        qerr[0] = 0;
        for (int i = 2; i < a.count && !res; i++) {
          if (!strcmp(a.items[i], "--clear")) {
            clear = 1;
            writes++;
          } else if (!strcmp(a.items[i], "--level") && i + 1 < a.count) {
            cJSON_DeleteItemFromObjectCaseSensitive(q, "level");
            cJSON_AddStringToObject(q, "level", a.items[++i]);
            writes++;
          } else if (!strcmp(a.items[i], "--unit") && i + 1 < a.count) {
            cJSON_DeleteItemFromObjectCaseSensitive(q, "unit");
            cJSON_AddStringToObject(q, "unit", a.items[++i]);
            writes++;
          } else if ((!strcmp(a.items[i], "--min") || !strcmp(a.items[i], "--max")) &&
                     i + 1 < a.count) {
            const char *key = a.items[i] + 2;
            double d = 0;
            if (!vc_parse_double(a.items[i + 1], &d)) {
              res = res_fail("measure: --%s must be a number, got '%s'", key,
                             a.items[i + 1]);
            } else {
              cJSON_DeleteItemFromObjectCaseSensitive(q, key);
              cJSON_AddNumberToObject(q, key, d);
              writes++;
            }
            i++;
          } else if (a.items[i][0] != '-') {
            res = res_fail("measure: unexpected argument '%s'", a.items[i]);
          }
        }
        if (res) {
          cJSON_Delete(q);
        } else if (!writes) { /* a bare `measure <ref>` reads */
          cJSON *cur = cJSON_GetObjectItemCaseSensitive(r, "quantity");
          char *s = cur ? cJSON_PrintUnformatted(cur) : NULL;
          res = res_make(1);
          res_line(res, "%s", s ? s : "null");
          free(s);
          res_set_data(res, cur ? cJSON_Duplicate(cur, 1) : cJSON_CreateNull());
          cJSON_Delete(q);
        } else if (clear) {
          cJSON_DeleteItemFromObjectCaseSensitive(r, "quantity");
          cJSON_Delete(q);
          res = res_make(1);
          res_line(res, "cleared the quantity of %s", vc_rune_name(r));
        } else if (!vc_quantity_validate(q, qerr, sizeof qerr)) {
          cJSON_Delete(q);
          res = res_fail("measure: %s", qerr);
        } else {
          cJSON_DeleteItemFromObjectCaseSensitive(r, "quantity");
          cJSON_AddItemToObject(r, "quantity", q);
          char *s = cJSON_PrintUnformatted(q);
          res = res_make(1);
          res_line(res, "%s: %s", vc_rune_name(r), s ? s : "{}");
          free(s);
          res_set_data(res, cJSON_Duplicate(q, 1));
        }
      }
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

  } else if (!strcmp(v, "place")) {
    /* SPEC §3.2/§6/§7 — the view slice:
     *   place <ref>                  -> read placement (query; data = value|null)
     *   place <ref> <x> <y> [<z>]   -> set {"x":n,"y":n[,"z":n]}
     *   place <ref> --clear          -> null
     * Single rune only (a position is per-rune; no @-multi). Mutates the view
     * slice: the router logs it on the mutation spine but takes NO undo frame,
     * and undo/redo never changes a surviving rune's placement (undo.c). */
    cJSON *mt = need_mantle(state, &err);
    if (!mt) {
      res = err;
    } else if (a.count < 2) {
      res = res_fail("usage: place <rune> [<x> <y> [<z>] | --clear]");
    } else {
      cJSON *r = vc_mantle_find_rune(mt, a.items[1]);
      /* Collect coordinate tokens past any `--flag` (a `$(…)` capture appends
       * --json; same pass-through convention as `ls`). Negative coords ("-5")
       * are single-dash and still count. */
      int clear = 0, bad = 0, ncoord = 0;
      double xyz[3];
      for (int i = 2; i < a.count; i++) {
        if (!strcmp(a.items[i], "--clear")) { clear = 1; continue; }
        if (a.items[i][0] == '-' && a.items[i][1] == '-') continue;
        if (ncoord >= 3) { bad = 1; break; }
        char *end = NULL;
        xyz[ncoord] = strtod(a.items[i], &end);
        if (!end || end == a.items[i] || *end != 0) { bad = 1; break; }
        ncoord++;
      }
      if (!r) {
        res = res_fail("no such rune: %s", a.items[1]);
      } else if (clear && ncoord == 0 && !bad) {
        cJSON *nul = cJSON_CreateNull();
        if (!cJSON_ReplaceItemInObjectCaseSensitive(r, "placement", nul))
          cJSON_AddItemToObject(r, "placement", nul);
        res = res_make(1);
        res_line(res, "%s placement cleared", vc_rune_name(r));
      } else if (ncoord == 0 && !bad && !clear) { /* query */
        cJSON *p = cJSON_GetObjectItemCaseSensitive(r, "placement");
        char *ps = (p && !cJSON_IsNull(p)) ? cJSON_PrintUnformatted(p) : NULL;
        res = res_make(1);
        res_line(res, "%s @ %s", vc_rune_name(r), ps ? ps : "(unplaced)");
        free(ps);
        res_set_data(res, p ? cJSON_Duplicate(p, 1) : cJSON_CreateNull());
      } else if (bad || ncoord == 1 || clear) {
        res = res_fail("usage: place <rune> [<x> <y> [<z>] | --clear] "
                       "(coordinates must be numbers)");
      } else {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "x", xyz[0]);
        cJSON_AddNumberToObject(p, "y", xyz[1]);
        if (ncoord == 3) cJSON_AddNumberToObject(p, "z", xyz[2]);
        cJSON *dup = cJSON_Duplicate(p, 1);
        if (!cJSON_ReplaceItemInObjectCaseSensitive(r, "placement", dup))
          cJSON_AddItemToObject(r, "placement", dup);
        char *ps = cJSON_PrintUnformatted(p);
        res = res_make(1);
        res_line(res, "%s @ %s", vc_rune_name(r), ps ? ps : "");
        free(ps);
        res_set_data(res, p);
      }
    }

  }
  return res;
}
