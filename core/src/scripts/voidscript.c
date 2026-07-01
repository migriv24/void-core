/* voidscript.c — the Voidscript interpreter (SPEC §8), run over the dispatcher.
 *
 * v0 subset (covers the common script surface):
 *   comments (#), ; / newline separators, { } blocks, every non-control line is a
 *   dispatcher command, `let`, $var / ${var} / $1.. / $@ / $? interpolation,
 *   $(command) capture (text, or `data` when --json is present), echo/print,
 *   if/elif/else, while (guarded), repeat, foreach v in (cmd), break/continue,
 *   return, halt, assert, and the operators == != < > <= >= && || ! in conditions.
 *
 * Deferred (noted in core/README): def/functions, try/catch, on error, include,
 * call, wait, prompt.
 */
#include "voidscript_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── statement parser + executor (the interpreter core) ── */
static void skip_sep(const char *s, int *p, int end) {
  for (;;) {
    while (*p < end && (s[*p] == ' ' || s[*p] == '\t' || s[*p] == '\r' ||
                        s[*p] == '\n' || s[*p] == ';'))
      (*p)++;
    if (*p < end && s[*p] == '#') {
      while (*p < end && s[*p] != '\n') (*p)++;
      continue;
    }
    break;
  }
}
/* read a statement header into buf; stop (unconsumed) at newline/;/{/}/end */
static void read_header(const char *s, int *p, int end, char *buf, int bufsz) {
  int n = 0;
  char q = 0;
  while (*p < end) {
    char ch = s[*p];
    if (q) {
      if (ch == q) q = 0;
      if (n < bufsz - 1) buf[n++] = ch;
      (*p)++;
      continue;
    }
    if (ch == '\'' || ch == '"') { q = ch; if (n < bufsz - 1) buf[n++] = ch; (*p)++; continue; }
    if (ch == '\n' || ch == ';' || ch == '{' || ch == '}') break;
    if (n < bufsz - 1) buf[n++] = ch;
    (*p)++;
  }
  buf[n] = 0;
}
/* *p must be at '{'; sets inner [is,ie) and advances *p past matching '}' */
static void find_block(const char *s, int *p, int end, int *is_, int *ie) {
  (*p)++;
  *is_ = *p;
  int depth = 1;
  char q = 0;
  while (*p < end && depth > 0) {
    char ch = s[*p];
    if (q) { if (ch == q) q = 0; (*p)++; continue; }
    if (ch == '\'' || ch == '"') { q = ch; (*p)++; continue; }
    if (ch == '{') depth++;
    else if (ch == '}') { depth--; if (depth == 0) break; }
    (*p)++;
  }
  *ie = *p;
  if (*p < end) (*p)++; /* consume '}' */
}
static void first_word(const char *buf, char *w, int wsz) {
  int i = 0;
  while (buf[i] == ' ' || buf[i] == '\t') i++;
  int n = 0;
  while (buf[i] && buf[i] != ' ' && buf[i] != '\t' && n < wsz - 1) w[n++] = buf[i++];
  w[n] = 0;
}
static const char *rest_after(const char *buf) {
  int i = 0;
  while (buf[i] == ' ' || buf[i] == '\t') i++;
  while (buf[i] && buf[i] != ' ' && buf[i] != '\t') i++;
  while (buf[i] == ' ' || buf[i] == '\t') i++;
  return buf + i;
}

static void exec_region(VS *c, const char *s, int start, int end);

static void do_let(VS *c, const char *rest) {
  char name[64];
  int i = 0, n = 0;
  while (rest[i] == ' ') i++;
  while (rest[i] && rest[i] != ' ' && rest[i] != '=' && n < 63) name[n++] = rest[i++];
  name[n] = 0;
  while (rest[i] == ' ') i++;
  if (rest[i] == '=') i++;
  while (rest[i] == ' ') i++;
  char val[1024];
  interpolate(c, rest + i, val, sizeof val);
  char *v = val;
  int L = (int)strlen(v);
  if (L >= 2 && ((v[0] == '"' && v[L - 1] == '"') || (v[0] == '\'' && v[L - 1] == '\''))) {
    v[L - 1] = 0;
    v++;
  }
  set_var(c, name, v);
}

static void loop_flags(VS *c, int *stop) {
  if (c->cont) c->cont = 0;
  if (c->brk) { c->brk = 0; *stop = 1; }
  if (c->ret || c->halt) *stop = 1;
}

static void do_foreach(VS *c, const char *s, const char *rest, int *p, int end) {
  char var[64];
  int i = 0, n = 0;
  while (rest[i] == ' ') i++;
  while (rest[i] && rest[i] != ' ' && n < 63) var[n++] = rest[i++];
  var[n] = 0;
  while (rest[i] == ' ') i++;
  if (!strncmp(rest + i, "in", 2)) i += 2;
  while (rest[i] == ' ') i++;
  char cmd[1024];
  cmd[0] = 0;
  if (rest[i] == '(') {
    i++;
    int d = 1, cn = 0;
    while (rest[i] && d > 0) {
      if (rest[i] == '(') d++;
      else if (rest[i] == ')') { d--; if (d == 0) break; }
      if (cn < 1023) cmd[cn++] = rest[i];
      i++;
    }
    cmd[cn] = 0;
  }
  int bs = *p, be = *p;
  if (*p < end && s[*p] == '{') find_block(s, p, end, &bs, &be);
  char ic[1024];
  interpolate(c, cmd, ic, sizeof ic);
  cJSON *r = vc_dispatch_json(c->m, ic);
  cJSON *data = cJSON_GetObjectItemCaseSensitive(r, "data");
  int stop = 0;
  if (cJSON_IsArray(data)) {
    cJSON *el = NULL;
    cJSON_ArrayForEach(el, data) {
      char val[256];
      if (cJSON_IsString(el)) snprintf(val, sizeof val, "%s", el->valuestring);
      else { char *sx = cJSON_PrintUnformatted(el); snprintf(val, sizeof val, "%s", sx ? sx : ""); free(sx); }
      set_var(c, var, val);
      exec_region(c, s, bs, be);
      loop_flags(c, &stop);
      if (stop) break;
    }
  } else {
    cJSON *ln = NULL;
    cJSON_ArrayForEach(ln, cJSON_GetObjectItemCaseSensitive(r, "lines")) {
      if (!cJSON_IsString(ln)) continue;
      set_var(c, var, ln->valuestring);
      exec_region(c, s, bs, be);
      loop_flags(c, &stop);
      if (stop) break;
    }
  }
  cJSON_Delete(r);
}

static void exec_region(VS *c, const char *s, int start, int end) {
  int p = start;
  while (p < end) {
    if (c->ret || c->halt || c->brk || c->cont) break;
    if (++c->steps > 2000000) { c->halt = 1; c->halt_code = 1; vs_append(c, "voidscript: step limit exceeded"); break; }
    skip_sep(s, &p, end);
    if (p >= end) break;

    char buf[2048];
    read_header(s, &p, end, buf, sizeof buf);
    char w[64];
    first_word(buf, w, sizeof w);

    if (!*w) {
      if (p < end && s[p] == '{') { int bs, be; find_block(s, &p, end, &bs, &be); exec_region(c, s, bs, be); }
      else if (p < end) p++;
      continue;
    }

    if (!strcmp(w, "let")) {
      do_let(c, rest_after(buf));
    } else if (!strcmp(w, "echo") || !strcmp(w, "print")) {
      char o[2048];
      interpolate(c, rest_after(buf), o, sizeof o);
      int L = (int)strlen(o);
      while (L > 0 && (o[L - 1] == ' ' || o[L - 1] == '\t')) o[--L] = 0; /* rtrim */
      if (L >= 2 && o[0] == '"' && o[L - 1] == '"') { o[L - 1] = 0; vs_append(c, o + 1); }
      else vs_append(c, o);
    } else if (!strcmp(w, "return")) {
      interpolate(c, rest_after(buf), c->retval, sizeof c->retval);
      c->ret = 1;
    } else if (!strcmp(w, "halt")) {
      char o[64];
      interpolate(c, rest_after(buf), o, sizeof o);
      c->halt = 1;
      c->halt_code = *o ? atoi(o) : 0;
    } else if (!strcmp(w, "break")) {
      c->brk = 1;
    } else if (!strcmp(w, "continue")) {
      c->cont = 1;
    } else if (!strcmp(w, "assert")) {
      if (!eval_cond(c, rest_after(buf))) {
        char m[320];
        snprintf(m, sizeof m, "assertion failed: %.280s", rest_after(buf));
        vs_append(c, m);
        c->halt = 1;
        c->halt_code = 1;
      }
    } else if (!strcmp(w, "if")) {
      int is_ = p, ie = p;
      if (p < end && s[p] == '{') find_block(s, &p, end, &is_, &ie);
      int done = 0;
      if (eval_cond(c, rest_after(buf))) { exec_region(c, s, is_, ie); done = 1; }
      for (;;) {
        int save = p;
        skip_sep(s, &p, end);
        char buf2[2048];
        read_header(s, &p, end, buf2, sizeof buf2);
        char w2[64];
        first_word(buf2, w2, sizeof w2);
        if (!strcmp(w2, "elif")) {
          int bs = p, be = p;
          if (p < end && s[p] == '{') find_block(s, &p, end, &bs, &be);
          if (!done && eval_cond(c, rest_after(buf2))) { exec_region(c, s, bs, be); done = 1; }
        } else if (!strcmp(w2, "else")) {
          int bs = p, be = p;
          if (p < end && s[p] == '{') find_block(s, &p, end, &bs, &be);
          if (!done) exec_region(c, s, bs, be);
          break;
        } else {
          p = save;
          break;
        }
      }
    } else if (!strcmp(w, "while")) {
      int bs = p, be = p;
      if (p < end && s[p] == '{') find_block(s, &p, end, &bs, &be);
      int stop = 0, guard = 0;
      while (eval_cond(c, rest_after(buf))) {
        exec_region(c, s, bs, be);
        loop_flags(c, &stop);
        if (stop || ++guard > 1000000) break;
      }
    } else if (!strcmp(w, "repeat")) {
      char o[64];
      interpolate(c, rest_after(buf), o, sizeof o);
      int reps = atoi(o);
      int bs = p, be = p;
      if (p < end && s[p] == '{') find_block(s, &p, end, &bs, &be);
      int stop = 0;
      for (int k = 0; k < reps; k++) {
        exec_region(c, s, bs, be);
        loop_flags(c, &stop);
        if (stop) break;
      }
    } else if (!strcmp(w, "foreach")) {
      do_foreach(c, s, rest_after(buf), &p, end);
    } else { /* a dispatcher command */
      char o[2048];
      interpolate(c, buf, o, sizeof o);
      cJSON *r = vc_dispatch_json(c->m, o);
      c->ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "ok"));
      cJSON *ln = NULL;
      cJSON_ArrayForEach(ln, cJSON_GetObjectItemCaseSensitive(r, "lines"))
        if (cJSON_IsString(ln)) vs_append(c, ln->valuestring);
      cJSON_Delete(r);
    }
  }
}

cJSON *vc_script_run(VC_Manager *m, const char *source, cJSON *args) {
  VS c;
  c.m = m;
  c.vars = cJSON_CreateObject();
  int own_args = !args;
  c.args = args ? args : cJSON_CreateArray();
  c.out = cJSON_CreateArray();
  c.ret = c.halt = c.halt_code = c.brk = c.cont = 0;
  c.ok = 1;
  c.steps = 0;
  c.retval[0] = 0;
  int len = source ? (int)strlen(source) : 0;
  exec_region(&c, source ? source : "", 0, len);

  cJSON *res = cJSON_CreateObject();
  cJSON_AddBoolToObject(res, "ok", c.halt_code == 0);
  cJSON_AddItemToObject(res, "lines", c.out);
  cJSON_AddItemToObject(res, "data",
                        c.retval[0] ? cJSON_CreateString(c.retval) : cJSON_CreateNull());
  cJSON_Delete(c.vars);
  if (own_args) cJSON_Delete(c.args);
  return res;
}

