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
static etok et_tokenize(const char *s) {
  etok t = {0};
  while (*s) {
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) break;
    if (*s == '(' || *s == ')') { char b[2] = {*s, 0}; et_push(&t, b, 1); s++; continue; }
    if (*s == '!' && s[1] == '=') { et_push(&t, "!=", 2); s += 2; continue; }
    if (*s == '=' && s[1] == '=') { et_push(&t, "==", 2); s += 2; continue; }
    if (*s == '<' && s[1] == '=') { et_push(&t, "<=", 2); s += 2; continue; }
    if (*s == '>' && s[1] == '=') { et_push(&t, ">=", 2); s += 2; continue; }
    if (*s == '&' && s[1] == '&') { et_push(&t, "&&", 2); s += 2; continue; }
    if (*s == '|' && s[1] == '|') { et_push(&t, "||", 2); s += 2; continue; }
    if (*s == '<' || *s == '>' || *s == '!') { char b[2] = {*s, 0}; et_push(&t, b, 1); s++; continue; }
    if (*s == '"') { const char *st = ++s; while (*s && *s != '"') s++; et_push(&t, st, (int)(s - st)); if (*s) s++; continue; }
    const char *st = s;
    while (*s && *s != ' ' && *s != '\t' && *s != '(' && *s != ')' && *s != '!' &&
           *s != '=' && *s != '<' && *s != '>' && *s != '&' && *s != '|')
      s++;
    et_push(&t, st, (int)(s - st));
  }
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
  char buf[2048];
  interpolate(c, text, buf, sizeof buf);
  etok t = et_tokenize(buf);
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
  } else { /* bare command: truthy iff ok */
    cJSON *r = vc_dispatch_json(c->m, buf);
    truth = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r, "ok"));
    cJSON_Delete(r);
  }
  et_free(&t);
  return truth;
}

/* ── statement scanner ───────────────────────────────────────────────────── */
