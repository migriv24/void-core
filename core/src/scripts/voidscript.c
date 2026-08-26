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
/* read a statement header into buf; stop (unconsumed) at newline/;/{/}/end.
 *
 * Quote state comes from vc_quote_step — the ONE §6.1 automaton (dispatch/args.c)
 * — and NOT from a local tracker. This reader used to carry its own, which had no
 * \' escape, so a §6.1-correct argument containing an apostrophe closed its quote
 * here (but not in the argv tokenizer), after which a newline or `;` inside the
 * VALUE terminated the statement and the remainder ran as commands, ok:true. A
 * stranger's text was therefore executable through any transcript. The header
 * keeps the raw bytes (quotes included) because vc_argv_split decodes them later;
 * only the automaton's quote state is consumed here. */
static char *read_header(const char *s, int *p, int end) {
  size_t cap = 256, n = 0;
  char *buf = (char *)malloc(cap);
  if (!buf) return NULL;
  char q = 0;
#define RH_PUT(ch)                                                             \
  do {                                                                         \
    if (n + 1 >= cap) { cap *= 2; buf = (char *)realloc(buf, cap); }           \
    buf[n++] = (char)(ch);                                                     \
  } while (0)
  while (*p < end) {
    char ch = s[*p];
    /* `${name}` is one expansion, not a block: the brace belongs to the
     * interpolator. Without this the reader ended the statement at the `{`, so
     * ${x} — a SPEC §8 core-subset feature — never worked in this core at all;
     * the braced half of conformance case 05 was passing on an accident. */
    if (!q && ch == '$' && *p + 1 < end && s[*p + 1] == '{') {
      while (*p < end) {
        char cc = s[*p];
        RH_PUT(cc);
        (*p)++;
        if (cc == '}') break;
      }
      continue;
    }
    if (!q && (ch == '\n' || ch == ';' || ch == '{' || ch == '}')) break;
    int emit, used = vc_quote_step(s + *p, &q, &emit);
    if (used == 0) break;
    if (*p + used > end) used = end - *p;
    for (int k = 0; k < used; k++) RH_PUT(s[*p + k]);
    *p += used;
  }
  if (n + 1 >= cap) { cap += 1; buf = (char *)realloc(buf, cap); }
  buf[n] = 0;
#undef RH_PUT
  return buf;
}
/* *p must be at '{'; sets inner [is,ie) and advances *p past matching '}' */
static void find_block(const char *s, int *p, int end, int *is_, int *ie) {
  (*p)++;
  *is_ = *p;
  int depth = 1;
  char q = 0; /* §6.1 quote state, via the shared automaton — see read_header */
  while (*p < end && depth > 0) {
    char ch = s[*p];
    if (!q) {
      if (ch == '$' && *p + 1 < end && s[*p + 1] == '{') { /* an expansion, not a block */
        while (*p < end && s[*p] != '}') (*p)++;
        if (*p < end) (*p)++;
        continue;
      }
      if (ch == '{') { depth++; (*p)++; continue; }
      if (ch == '}') { depth--; if (depth == 0) break; (*p)++; continue; }
    }
    int emit, used = vc_quote_step(s + *p, &q, &emit);
    if (used == 0) break;
    *p += used;
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
  /* `let x = <rest>`: the rest of the statement, with trailing SOURCE whitespace
   * trimmed (it is layout, not value — a `let` closing a `{ }` block on one line
   * used to capture the space before the brace), decoded as exactly one §6.1
   * argument. Trimming happens on the source, before interpolation, so a value
   * that genuinely ends in a space survives. */
  size_t sl = strlen(rest + i);
  char *src = (char *)malloc(sl + 1);
  memcpy(src, rest + i, sl + 1);
  while (sl > 0 && (src[sl - 1] == ' ' || src[sl - 1] == '\t')) src[--sl] = 0;
  unsigned char *vm = NULL;
  char *val = vs_interp_alloc(c, src, &vm);
  char *v = vs_decode_one(val ? val : "", vm, NULL);
  set_var(c, name, v ? v : "");
  free(v);
  free(val);
  free(vm);
  free(src);
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

    char *buf = read_header(s, &p, end);
    if (!buf) break;
    char w[64];
    first_word(buf, w, sizeof w);

    if (!*w) {
      free(buf);
      if (p < end && s[p] == '{') { int bs, be; find_block(s, &p, end, &bs, &be); exec_region(c, s, bs, be); }
      else if (p < end) p++;
      continue;
    }

    if (!strcmp(w, "let")) {
      do_let(c, rest_after(buf));
    } else if (!strcmp(w, "echo") || !strcmp(w, "print")) {
      char *o = vs_interp_alloc(c, rest_after(buf), NULL);
      int L = o ? (int)strlen(o) : 0;
      while (L > 0 && (o[L - 1] == ' ' || o[L - 1] == '\t')) o[--L] = 0; /* rtrim */
      if (L >= 2 && o[0] == '"' && o[L - 1] == '"') { o[L - 1] = 0; vs_append(c, o + 1); }
      else vs_append(c, o ? o : "");
      free(o);
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
        char *buf2 = read_header(s, &p, end);
        if (!buf2) { p = save; break; }
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
          free(buf2);
          break;
        } else {
          p = save;
          free(buf2);
          break;
        }
        free(buf2);
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
      /* Interpolate with provenance, then re-emit as a canonical command line:
       * an expanded value is exactly one argument and can never become syntax
       * (SPEC §8, "Quoting and expansion"). */
      unsigned char *mk = NULL;
      char *o = vs_interp_alloc(c, buf, &mk);
      int unterm = 0;
      char *cl = vs_command_line(o ? o : "", mk, &unterm);
      if (unterm) {
        /* §6.1 rule 5: this statement's quote never closed, so it has eaten the
         * rest of the transcript. Halt instead of executing the wreckage. */
        free(cl);
        free(o);
        free(mk);
        vs_append(c, "voidscript: unterminated quote (SPEC §6.1) — the rest of "
                     "the script was swallowed by this statement");
        c->ok = 0;
        c->halt = 1;
        c->halt_code = 1;
        free(buf);
        break;
      }
      cJSON *r = vc_dispatch_json(c->m, cl ? cl : (o ? o : ""));
      free(cl);
      free(o);
      free(mk);
      c->ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "ok"));
      cJSON *ln = NULL;
      cJSON_ArrayForEach(ln, cJSON_GetObjectItemCaseSensitive(r, "lines"))
        if (cJSON_IsString(ln)) vs_append(c, ln->valuestring);
      cJSON_Delete(r);
    }
    free(buf);
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

