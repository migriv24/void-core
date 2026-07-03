/* args.c — quote-aware argv tokenizer (SPEC §6). Splits a command line on
 * whitespace, respecting single and double quotes. The per-token buffer grows
 * dynamically (no length cap), so large quoted payloads — e.g. a `batch` of many
 * commands — are never truncated. */
#include "vc_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
  size_t n = strlen(s) + 1;
  char *d = (char *)malloc(n);
  if (d) memcpy(d, s, n);
  return d;
}

vc_argv vc_argv_split(const char *line) {
  vc_argv a;
  a.items = NULL;
  a.count = 0;
  int cap = 0;
  if (!line) return a;

  const char *p = line;
  size_t bcap = 256;
  char *buf = (char *)malloc(bcap);
  while (*p) {
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) break;

    size_t n = 0;
    char quote = 0;
    while (*p) {
      char c = *p;
      if (quote) {
        /* inside single quotes, \' is a literal quote (so arbitrary text/JSON can
         * be passed); every other backslash stays literal (don't clash with JSON
         * or Deltarune text codes like \cY). */
        if (quote == '\'' && c == '\\' && p[1] == '\'') {
          if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }
          buf[n++] = '\'';
          p += 2;
          continue;
        }
        if (c == quote) { quote = 0; p++; continue; }
      } else {
        if (c == '\'' || c == '"') { quote = c; p++; continue; }
        if (isspace((unsigned char)c)) break;
      }
      if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }
      buf[n++] = c;
      p++;
    }
    if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }
    buf[n] = 0;

    if (a.count >= cap) {
      cap = cap ? cap * 2 : 8;
      a.items = (char **)realloc(a.items, (size_t)cap * sizeof(char *));
    }
    a.items[a.count++] = dupstr(buf);
  }
  free(buf);
  return a;
}

/* ── POSIX surface (SPEC §7) ─────────────────────────────────────────────────
 * Aliases are argument-aware desugarings applied to the argv before routing —
 * one semantics, many spellings; an alias never forks behavior. Rewriting here
 * (before is_mutating / undo capture) keeps the alias and its canonical form
 * indistinguishable everywhere downstream. */
static void argv_replace(vc_argv *a, int i, const char *s) {
  free(a->items[i]);
  a->items[i] = dupstr(s);
}

static void argv_prepend(vc_argv *a, const char *s) {
  a->items = (char **)realloc(a->items, (size_t)(a->count + 1) * sizeof(char *));
  memmove(&a->items[1], &a->items[0], (size_t)a->count * sizeof(char *));
  a->items[0] = dupstr(s);
  a->count++;
}

void vc_argv_desugar(vc_argv *a) {
  if (a->count == 0) return;
  const char *v = a->items[0];
  /* verb renames */
  static const struct { const char *from, *to; } ren[] = {
      {"?", "help"},     {"man", "help"},  {"quit", "exit"},
      {"pwd", "where"},  {"dump", "export"}, {"grep", "find"},
      {"cd", "use"},
  };
  for (size_t i = 0; i < sizeof ren / sizeof ren[0]; i++) {
    if (!strcmp(v, ren[i].from)) { argv_replace(a, 0, ren[i].to); return; }
  }
  /* argument-aware desugarings: `rm x` means `rune rm x` (not `rune x`) */
  static const struct { const char *from, *fam, *sub; } des[] = {
      {"rm", "rune", "rm"},   {"mv", "rune", "rename"},
      {"cp", "rune", "dup"},  {"mkdir", "mantle", "new"},
  };
  for (size_t i = 0; i < sizeof des / sizeof des[0]; i++) {
    if (!strcmp(v, des[i].from)) {
      argv_replace(a, 0, des[i].sub);
      argv_prepend(a, des[i].fam);
      return;
    }
  }
}

void vc_argv_free(vc_argv *a) {
  if (!a) return;
  for (int i = 0; i < a->count; i++) free(a->items[i]);
  free(a->items);
  a->items = NULL;
  a->count = 0;
}
