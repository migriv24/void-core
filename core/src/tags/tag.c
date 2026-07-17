/* tag.c — tag matching + the filter-expression grammar (SPEC §5).
 *
 * A rune matches a TAG if the tag is in its `tags` array, OR equals its
 * spirit.name, OR equals "glyph:<its glyph>". (Name-as-tag is SPEC §5;
 * glyph-as-tag is the unification from notes/concept-brainstorm.md — the core
 * sees a rune as a bag of tags, three of which are special.)
 *
 * Grammar (SPEC §5), with one ergonomic extension: adjacency = implicit AND.
 *   expr := or
 *   or   := and ( ("OR"|"||") and )*
 *   and  := not ( ("AND"|"&&") not | <atom> )*      // implicit AND on adjacency
 *   not  := ("NOT"|"!") not | atom
 *   atom := "(" or ")" | TAG
 * Operators are case-insensitive. An empty expression matches all runes.
 *
 * Tokenization matches the JS oracle (src/tags/tags.js): `&&`, `||`, `!` are
 * operators only at a token boundary; inside a word they are ordinary tag
 * characters (so `a&b` is ONE tag atom, not `a AND b`). Only whitespace and
 * parentheses split a word. A lone `&` or `|` is therefore a (never-matching)
 * tag, never a crash — the previous tokenizer looped forever on it.
 */
#include "vc_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int ci_eq(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return *a == *b;
}

/* ── tag membership ──────────────────────────────────────────────────────── */
int vc_rune_has_tag(const cJSON *rune, const char *tag) {
  if (!tag || !*tag) return 0;
  if (!strcmp(vc_rune_name(rune), tag)) return 1; /* name is a tag */
  cJSON *gj = cJSON_GetObjectItemCaseSensitive((cJSON *)rune, "glyph");
  if (cJSON_IsString(gj) && !strncmp(tag, "glyph:", 6) &&
      !strcmp(tag + 6, gj->valuestring))
    return 1; /* glyph is the reserved tag glyph:<name> */
  cJSON *tags = cJSON_GetObjectItemCaseSensitive((cJSON *)rune, "tags");
  cJSON *it = NULL;
  cJSON_ArrayForEach(it, tags) {
    if (cJSON_IsString(it) && !strcmp(it->valuestring, tag)) return 1;
  }
  return 0;
}

/* ── tokenizer ───────────────────────────────────────────────────────────── */
typedef struct {
  char **toks;
  int count, cap;
} toklist;

static void tok_push(toklist *t, const char *s, int len) {
  if (t->count >= t->cap) {
    t->cap = t->cap ? t->cap * 2 : 16;
    t->toks = (char **)realloc(t->toks, (size_t)t->cap * sizeof(char *));
  }
  char *d = (char *)malloc((size_t)len + 1);
  memcpy(d, s, (size_t)len);
  d[len] = 0;
  t->toks[t->count++] = d;
}

static toklist tokenize(const char *expr) {
  toklist t = {0};
  const char *p = expr;
  while (*p) {
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) break;
    char c = *p;
    if (c == '(' || c == ')' || c == '!') { tok_push(&t, p, 1); p++; continue; }
    if (c == '&' && p[1] == '&') { tok_push(&t, "&&", 2); p += 2; continue; }
    if (c == '|' && p[1] == '|') { tok_push(&t, "||", 2); p += 2; continue; }
    /* word: runs to whitespace/paren — `&`, `|`, `!` mid-word are tag chars
     * (oracle behavior; also guarantees the scan always advances). */
    const char *start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '(' && *p != ')') p++;
    tok_push(&t, start, (int)(p - start));
  }
  return t;
}

static void tok_free(toklist *t) {
  for (int i = 0; i < t->count; i++) free(t->toks[i]);
  free(t->toks);
}

/* ── token classification ────────────────────────────────────────────────── */
static int is_or(const char *s) { return !strcmp(s, "||") || ci_eq(s, "OR"); }
static int is_and(const char *s) { return !strcmp(s, "&&") || ci_eq(s, "AND"); }
static int is_not(const char *s) { return !strcmp(s, "!") || ci_eq(s, "NOT"); }
static int starts_atom(const char *s) {
  return !is_or(s) && !is_and(s) && strcmp(s, ")") != 0;
}

/* ── recursive-descent evaluator (evaluates against `rune` in place) ─────── */
static int ev_or(toklist *t, int *p, const cJSON *rune);

static int ev_atom(toklist *t, int *p, const cJSON *rune) {
  if (*p >= t->count) return 0;
  const char *s = t->toks[*p];
  if (!strcmp(s, "(")) {
    (*p)++;
    int r = ev_or(t, p, rune);
    if (*p < t->count && !strcmp(t->toks[*p], ")")) (*p)++;
    return r;
  }
  (*p)++;
  return vc_rune_has_tag(rune, s);
}

static int ev_not(toklist *t, int *p, const cJSON *rune) {
  if (*p < t->count && is_not(t->toks[*p])) {
    (*p)++;
    return !ev_not(t, p, rune);
  }
  return ev_atom(t, p, rune);
}

static int ev_and(toklist *t, int *p, const cJSON *rune) {
  int r = ev_not(t, p, rune);
  while (*p < t->count) {
    if (is_and(t->toks[*p])) {
      (*p)++;
      r = ev_not(t, p, rune) && r;
    } else if (starts_atom(t->toks[*p])) { /* implicit AND */
      r = ev_not(t, p, rune) && r;
    } else {
      break;
    }
  }
  return r;
}

static int ev_or(toklist *t, int *p, const cJSON *rune) {
  int r = ev_and(t, p, rune);
  while (*p < t->count && is_or(t->toks[*p])) {
    (*p)++;
    r = ev_and(t, p, rune) || r;
  }
  return r;
}

int vc_filter_eval(const cJSON *rune, const char *expr) {
  if (!expr) return 1;
  toklist t = tokenize(expr);
  if (t.count == 0) { tok_free(&t); return 1; } /* empty matches all */
  int p = 0;
  int r = ev_or(&t, &p, rune);
  tok_free(&t);
  return r;
}

/* ── fundamental axes (SPEC §5) ──────────────────────────────────────────── */
const char *vc_axis_of(const char *tag) {
  const char *colon = tag ? strchr(tag, ':') : NULL;
  if (!colon) return "free"; /* bare tag (incl. a rune name) has no namespace */
  char ns[64];
  size_t n = (size_t)(colon - tag);
  if (n >= sizeof ns) n = sizeof ns - 1;
  memcpy(ns, tag, n);
  ns[n] = 0;
  static const struct {
    const char *ns;
    const char *axis;
  } map[] = {
      {"site", "where"},   {"group", "where"},    {"section", "where"},
      {"outcome", "where"},{"where", "where"},     {"page", "where"},
      {"room", "where"},   {"location", "where"},  {"what", "what"},
      {"type", "what"},    {"kind", "what"},       {"category", "what"},
      {"who", "who"},      {"character", "who"},   {"actor", "who"},
      {"speaker", "who"},  {"author", "who"},      {"when", "when"},
      {"trigger", "when"}, {"month", "when"},      {"day", "when"},
      {"time", "when"},    {"chapter", "when"},    {"phase", "when"},
      {"state", "state"},  {"status", "state"},    {"flag", "state"},
      {"mood", "state"},
  };
  for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
    if (ci_eq(ns, map[i].ns)) return map[i].axis;
  return "free";
}
