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

void vc_argv_free(vc_argv *a) {
  if (!a) return;
  for (int i = 0; i < a->count; i++) free(a->items[i]);
  free(a->items);
  a->items = NULL;
  a->count = 0;
}
