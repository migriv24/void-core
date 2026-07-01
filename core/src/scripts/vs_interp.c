/* vs_interp.c — Voidscript variable store and $var / ${var} / $(command)
 * interpolation. Extracted from the original monolithic voidscript.c. */
#include "voidscript_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vs_append(VS *c, const char *line) {
  cJSON_AddItemToArray(c->out, cJSON_CreateString(line));
}
void set_var(VS *c, const char *name, const char *val) {
  cJSON_DeleteItemFromObjectCaseSensitive(c->vars, name);
  cJSON_AddStringToObject(c->vars, name, val);
}
static const char *get_var(VS *c, const char *name) {
  cJSON *v = cJSON_GetObjectItemCaseSensitive(c->vars, name);
  return cJSON_IsString(v) ? v->valuestring : NULL;
}

/* ── variable / capture interpolation ────────────────────────────────────── */
static void interp_capture(VS *c, const char *cmd, char *out, int *o, int outsz) {
  char ic[1024];
  interpolate(c, cmd, ic, sizeof ic);
  cJSON *r = vc_dispatch_json(c->m, ic);
  if (strstr(ic, "--json")) {
    char *s = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(r, "data"));
    for (int k = 0; s && s[k] && *o < outsz - 1; k++) out[(*o)++] = s[k];
    free(s);
  } else {
    cJSON *ln = NULL;
    int first = 1;
    cJSON_ArrayForEach(ln, cJSON_GetObjectItemCaseSensitive(r, "lines")) {
      if (!cJSON_IsString(ln)) continue;
      if (!first && *o < outsz - 1) out[(*o)++] = ' ';
      for (int k = 0; ln->valuestring[k] && *o < outsz - 1; k++)
        out[(*o)++] = ln->valuestring[k];
      first = 0;
    }
  }
  cJSON_Delete(r);
}

void interpolate(VS *c, const char *in, char *out, int outsz) {
  int o = 0;
  for (int i = 0; in[i] && o < outsz - 1;) {
    if (in[i] != '$') { out[o++] = in[i++]; continue; }
    if (in[i + 1] == '(') { /* $(command) capture */
      int j = i + 2, d = 1, cn = 0;
      char cmd[1024];
      while (in[j] && d > 0) {
        if (in[j] == '(') d++;
        else if (in[j] == ')') { d--; if (d == 0) break; }
        if (cn < 1023) cmd[cn++] = in[j];
        j++;
      }
      cmd[cn] = 0;
      interp_capture(c, cmd, out, &o, outsz);
      i = in[j] == ')' ? j + 1 : j;
      continue;
    }
    int braced = 0, j = i + 1;
    if (in[j] == '{') { braced = 1; j++; }
    char name[64];
    int n = 0;
    if (in[j] == '@' || in[j] == '?') { name[n++] = in[j++]; }
    else while (in[j] && (isalnum((unsigned char)in[j]) || in[j] == '_') && n < 63)
      name[n++] = in[j++];
    name[n] = 0;
    if (braced && in[j] == '}') j++;

    if (!strcmp(name, "@")) { /* all args, space-joined */
      cJSON *el = NULL;
      int first = 1;
      cJSON_ArrayForEach(el, c->args) {
        if (!first && o < outsz - 1) out[o++] = ' ';
        if (cJSON_IsString(el))
          for (int k = 0; el->valuestring[k] && o < outsz - 1; k++)
            out[o++] = el->valuestring[k];
        first = 0;
      }
    } else if (!strcmp(name, "?")) {
      char nb[16];
      snprintf(nb, sizeof nb, "%d", c->ok);
      for (int k = 0; nb[k] && o < outsz - 1; k++) out[o++] = nb[k];
    } else {
      const char *val = NULL;
      if (name[0] >= '0' && name[0] <= '9') {
        cJSON *el = cJSON_GetArrayItem(c->args, atoi(name) - 1);
        val = cJSON_IsString(el) ? el->valuestring : "";
      } else {
        val = get_var(c, name);
      }
      if (val)
        for (int k = 0; val[k] && o < outsz - 1; k++) out[o++] = val[k];
    }
    i = j;
  }
  out[o] = 0;
}

/* ── expression evaluation (for conditions) ──────────────────────────────── */
