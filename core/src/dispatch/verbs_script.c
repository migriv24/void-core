/* verbs_script.c — the "script" verb family for the dispatcher.
 * Handlers are grouped by family (SPEC §7); the router in dispatch.c tries each
 * family in turn. Bodies are unchanged from the original dispatch.c. */
#include "dispatch_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *vc_verbs_script(VC_Manager *m, cJSON *state, vc_argv a, const char *v) {
  cJSON *res = NULL;
  cJSON *err = NULL;
  (void)m; (void)state; (void)a; (void)v; (void)err;
  if (0) {
  } else if (!strcmp(v, "script")) {
    cJSON *scripts = cJSON_GetObjectItemCaseSensitive(state, "scripts");
    if (a.count >= 2 && !strcmp(a.items[1], "ls")) {
      res = res_make(1);
      cJSON *arr = cJSON_CreateArray();
      cJSON *e = NULL;
      cJSON_ArrayForEach(e, scripts) {
        res_line(res, "%s", e->string);
        cJSON_AddItemToArray(arr, cJSON_CreateString(e->string));
      }
      if (cJSON_GetArraySize(arr) == 0) res_line(res, "(no scripts)");
      res_set_data(res, arr);
    } else if (a.count >= 3 && !strcmp(a.items[1], "show")) {
      cJSON *src = cJSON_GetObjectItemCaseSensitive(scripts, a.items[2]);
      if (!cJSON_IsString(src)) {
        res = res_fail("no such script: %s", a.items[2]);
      } else {
        res = res_make(1);
        res_line(res, "%s", src->valuestring);
        res_set_data(res, cJSON_CreateString(src->valuestring));
      }
    } else if (a.count >= 3 &&
               (!strcmp(a.items[1], "set") || !strcmp(a.items[1], "new"))) {
      const char *src = a.count >= 4 ? a.items[3] : "";
      cJSON_DeleteItemFromObjectCaseSensitive(scripts, a.items[2]);
      cJSON_AddStringToObject(scripts, a.items[2], src);
      res = res_make(1);
      res_line(res, "saved script '%s'", a.items[2]);
    } else if (a.count >= 3 && !strcmp(a.items[1], "run")) {
      cJSON *src = cJSON_GetObjectItemCaseSensitive(scripts, a.items[2]);
      if (!cJSON_IsString(src)) {
        res = res_fail("no such script: %s", a.items[2]);
      } else {
        cJSON *sargs = cJSON_CreateArray();
        for (int i = 3; i < a.count; i++)
          cJSON_AddItemToArray(sargs, cJSON_CreateString(a.items[i]));
        res = vc_script_run(m, src->valuestring, sargs);
        cJSON_Delete(sargs);
      }
    } else {
      res = res_fail("usage: script run|ls|show|new|set <name> ...");
    }

  } else if (!strcmp(v, "batch")) {
    if (a.count < 2) {
      res = res_fail("usage: batch '<json array of command strings>'");
    } else {
      cJSON *cmds = cJSON_Parse(a.items[1]);
      if (!cmds || !cJSON_IsArray(cmds)) {
        if (cmds) cJSON_Delete(cmds);
        res = res_fail("batch expects a JSON array of command strings");
      } else {
        cJSON *save_m = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(state, "mantles"), 1);
        cJSON *save_a = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(state, "active"), 1);
        int prev = m->suppress_undo;
        m->suppress_undo = 1; /* one undo frame for the whole batch, not N */
        int failed = -1, i = 0;
        char failmsg[512];
        failmsg[0] = 0;
        cJSON *c = NULL;
        cJSON_ArrayForEach(c, cmds) {
          if (cJSON_IsString(c)) {
            cJSON *sub = vc_dispatch_json(m, c->valuestring);
            if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(sub, "ok"))) {
              cJSON *l0 = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(sub, "lines"), 0);
              snprintf(failmsg, sizeof failmsg, "%s",
                       cJSON_IsString(l0) ? l0->valuestring : "failed");
              failed = i;
              cJSON_Delete(sub);
              break;
            }
            cJSON_Delete(sub);
          }
          i++;
        }
        m->suppress_undo = prev;
        if (failed >= 0) { /* atomic: roll the model back */
          cJSON_ReplaceItemInObjectCaseSensitive(state, "mantles", save_m);
          cJSON_ReplaceItemInObjectCaseSensitive(state, "active", save_a);
          res = res_fail("batch rolled back at command %d: %s", failed, failmsg);
        } else {
          cJSON_Delete(save_m);
          cJSON_Delete(save_a);
          res = res_make(1);
          res_line(res, "batch applied %d command(s)", cJSON_GetArraySize(cmds));
        }
        cJSON_Delete(cmds);
      }
    }

  }
  return res;
}
