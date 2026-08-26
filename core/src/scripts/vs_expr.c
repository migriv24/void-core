/* vs_expr.c — the Voidscript condition-expression evaluator: tokenize a string
 * and evaluate the SPEC §8 operators (== != < > <= >= && || !) to a boolean.
 * Extracted from the original monolithic voidscript.c. */
#include "voidscript_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char **t; int n, cap; } etok;
static void et_push(etok *t, const char *s, int len) {
  if (t->n >= t->cap) {
    t->cap = t->cap ? t->cap * 2 : 16;
    t->t = (char **)realloc(t->t, (size_t)t->cap * sizeof(char *));
  }
  char *d = (char *)malloc((size_t)len + 1);
  memcpy(d, s, (size_t)len);
  d[len] = 0;
  t->t[t->n++] = d;
}
static void et_free(etok *t) {
  for (int i = 0; i < t->n; i++) free(t->t[i]);
  free(t->t);
}
/* Tokenize a condition. Two rules matter and both used to be wrong:
 *
 *  - A token that BEGINS with a quote is a quoted literal, read with the ONE
 *    §6.1 automaton (dispatch/args.c) and stripped of its quotes. The old lexer
 *    knew only `"` and had no \' escape, so a literal written the way the SPEC
 *    tells hosts to write it (`'don\'t'`) compared as the seven characters
 *    `'don\'t'` and never equalled `don't`. Conformance case 12 asserted exactly
 *    that and passed only because a SECOND bug (the statement reader running past
 *    the newline) made the comparison accidentally truthy.
 *
 *  - Bytes produced by an expansion (mask 1) are pure content: an apostrophe
 *    inside an interpolated value can never open a quoted run, a space inside one
 *    can never split a token, and an operator inside one is just text. Mask 2 is
 *    the zero-width field mark, so an empty expansion still yields an explicit
 *    empty token instead of vanishing and shifting every operator one place left.
 *
 * Note this is deliberately NOT the whole of §6.1: quoting here is
 * begins-the-token, not strip-anywhere, so a JSON literal (`["a","b"]`) stays a
 * bare word with its quotes intact — the idiom the suite compares `--json`
 * captures against. The two grammars agree on quoted tokens, which is the part a
 * host has to get right; they differ on bare words, which is stated in SPEC §8.
 *
 * `mask` may be NULL (then every byte is source syntax). */
static etok et_tokenize(const char *s, const unsigned char *mask) {
  etok t = {0};
  int i = 0;
#define EM(k) ((unsigned char)(mask ? mask[k] : 0))
  while (s[i]) {
    while (!EM(i) && (s[i] == ' ' || s[i] == '\t')) i++;
    if (!s[i]) break;
    if (!EM(i)) {
      const char *p = s + i;
      if (*p == '(' || *p == ')') { char b[2] = {*p, 0}; et_push(&t, b, 1); i++; continue; }
      if (*p == '!' && p[1] == '=') { et_push(&t, "!=", 2); i += 2; continue; }
      if (*p == '=' && p[1] == '=') { et_push(&t, "==", 2); i += 2; continue; }
      if (*p == '<' && p[1] == '=') { et_push(&t, "<=", 2); i += 2; continue; }
      if (*p == '>' && p[1] == '=') { et_push(&t, ">=", 2); i += 2; continue; }
      if (*p == '&' && p[1] == '&') { et_push(&t, "&&", 2); i += 2; continue; }
      if (*p == '|' && p[1] == '|') { et_push(&t, "||", 2); i += 2; continue; }
      if (*p == '<' || *p == '>' || *p == '!') { char b[2] = {*p, 0}; et_push(&t, b, 1); i++; continue; }
    }
    {
      size_t bcap = 64, n = 0;
      char *b = (char *)malloc(bcap);
#define EPUSH(ch)                                                              \
  do {                                                                         \
    if (n + 1 >= bcap) { bcap *= 2; b = (char *)realloc(b, bcap); }            \
    b[n++] = (char)(ch);                                                       \
  } while (0)
      if (!EM(i) && (s[i] == '\'' || s[i] == '"')) {
        /* quoted literal — §6.1 rules until the run closes */
        char q = 0;
        int emit, used = vc_quote_step(s + i, &q, &emit); /* opens it */
        i += used;
        while (s[i] && q) {
          if (EM(i)) { if (EM(i) == 1) EPUSH(s[i]); i++; continue; }
          used = vc_quote_step(s + i, &q, &emit);
          if (used == 0) break;
          i += used;
          if (emit >= 0) EPUSH(emit);
        }
      } else {
        /* bare word — quotes inside are literal (JSON survives) */
        while (s[i]) {
          unsigned char mv = EM(i);
          if (mv) { if (mv == 1) EPUSH(s[i]); i++; continue; }
          if (s[i] == ' ' || s[i] == '\t' || strchr("()!=<>&|", s[i])) break;
          EPUSH(s[i]);
          i++;
        }
      }
#undef EPUSH
      et_push(&t, b, (int)n);
      free(b);
    }
  }
#undef EM
  return t;
}

typedef struct { char s[256]; } Val;
static Val mkval(const char *x) { Val v; snprintf(v.s, sizeof v.s, "%s", x ? x : ""); return v; }
static int is_num(const char *s, double *d) {
  if (!*s) return 0;
  char *e;
  *d = strtod(s, &e);
  return *e == 0;
}
static int truthy(const char *s) {
  double d;
  if (is_num(s, &d)) return d != 0;
  return *s && strcmp(s, "false") && strcmp(s, "0");
}
static int is_cmp(const char *s) {
  return !strcmp(s, "==") || !strcmp(s, "!=") || !strcmp(s, "<") ||
         !strcmp(s, ">") || !strcmp(s, "<=") || !strcmp(s, ">=");
}

static Val ev_or(etok *t, int *p);
static Val ev_atom(etok *t, int *p) {
  if (*p >= t->n) return mkval("");
  const char *s = t->t[*p];
  if (!strcmp(s, "(")) {
    (*p)++;
    Val v = ev_or(t, p);
    if (*p < t->n && !strcmp(t->t[*p], ")")) (*p)++;
    return v;
  }
  (*p)++;
  return mkval(s);
}
static Val ev_cmp(etok *t, int *p) {
  Val l = ev_atom(t, p);
  if (*p < t->n && is_cmp(t->t[*p])) {
    const char *op = t->t[*p];
    (*p)++;
    Val r = ev_atom(t, p);
    double a, b;
    int na = is_num(l.s, &a), nb = is_num(r.s, &b), res;
    if (na && nb) {
      res = !strcmp(op, "==") ? a == b : !strcmp(op, "!=") ? a != b
          : !strcmp(op, "<")  ? a < b  : !strcmp(op, ">")  ? a > b
          : !strcmp(op, "<=") ? a <= b : a >= b;
    } else {
      int cc = strcmp(l.s, r.s);
      res = !strcmp(op, "==") ? cc == 0 : !strcmp(op, "!=") ? cc != 0
          : !strcmp(op, "<")  ? cc < 0  : !strcmp(op, ">")  ? cc > 0
          : !strcmp(op, "<=") ? cc <= 0 : cc >= 0;
    }
    return mkval(res ? "1" : "0");
  }
  return l;
}
static Val ev_not(etok *t, int *p) {
  if (*p < t->n && !strcmp(t->t[*p], "!")) {
    (*p)++;
    Val v = ev_not(t, p);
    return mkval(truthy(v.s) ? "0" : "1");
  }
  return ev_cmp(t, p);
}
static Val ev_and(etok *t, int *p) {
  Val l = ev_not(t, p);
  while (*p < t->n && !strcmp(t->t[*p], "&&")) {
    (*p)++;
    Val r = ev_not(t, p);
    l = mkval((truthy(l.s) && truthy(r.s)) ? "1" : "0");
  }
  return l;
}
static Val ev_or(etok *t, int *p) {
  Val l = ev_and(t, p);
  while (*p < t->n && !strcmp(t->t[*p], "||")) {
    (*p)++;
    Val r = ev_and(t, p);
    l = mkval((truthy(l.s) || truthy(r.s)) ? "1" : "0");
  }
  return l;
}

int eval_cond(VS *c, const char *text) {
  unsigned char *mask = NULL;
  char *buf = vs_interp_alloc(c, text, &mask);
  if (!buf) { free(mask); return 0; }
  etok t = et_tokenize(buf, mask);
  int has_op = 0;
  for (int i = 0; i < t.n; i++) {
    const char *s = t.t[i];
    if (is_cmp(s) || !strcmp(s, "&&") || !strcmp(s, "||") || !strcmp(s, "!")) {
      has_op = 1;
      break;
    }
  }
  int truth;
  if (has_op) {
    int p = 0;
    Val v = ev_or(&t, &p);
    truth = truthy(v.s);
  } else { /* bare command: truthy iff ok — re-emitted canonically, so an
             * interpolated value cannot become an extra argument or a flag */
    int unterm = 0;
    char *cl = vs_command_line(buf, mask, &unterm);
    cJSON *r = vc_dispatch_json(c->m, cl ? cl : buf);
    free(cl);
    truth = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "ok"));
    cJSON_Delete(r);
  }
  et_free(&t);
  free(buf);
  free(mask);
  return truth;
}

/* ── statement scanner ───────────────────────────────────────────────────── */
