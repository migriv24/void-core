/* args.c — the SPEC §6.1 quote automaton, and the argv tokenizer built on it.
 *
 * There is exactly ONE implementation of "am I inside a quoted run" in this tree
 * (`vc_quote_step`), and every scanner that needs the answer calls it: the argv
 * tokenizer below, and the Voidscript statement reader in scripts/voidscript.c.
 * That is deliberate and it is the fix for a real defect: the statement reader
 * used to carry its own two-line quote tracker with no \' escape, so a correctly
 * §6.1-quoted value containing an apostrophe closed its quote early and the rest
 * of the value was executed as commands (2026-08-25; reported in shape by Void
 * Hormiga, root-caused here). §6.1 is a specification standing in for a
 * component — this file is that component. A second quote scanner anywhere in
 * this tree is a bug, whatever its rules say.
 *
 * The per-token buffer grows dynamically (no length cap), so large quoted
 * payloads — e.g. a `batch` of many commands — are never truncated. */
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

/* ── the §6.1 quote automaton ────────────────────────────────────────────────
 * Consume one logical character at `p` given the current quote state.
 *
 *   *quote  in/out: the open quote character, or 0 when outside a quoted run.
 *   *emit   out:    the DECODED byte this step contributes, or -1 for none
 *                   (an opening/closing quote contributes nothing).
 *   returns:        how many raw input bytes were consumed (1 or 2; 0 at NUL).
 *
 * A caller that wants the decoded value (the argv tokenizer) appends *emit; a
 * caller that wants the raw slice back (the statement reader, which hands its
 * buffer on to the tokenizer) copies the consumed bytes verbatim. Structural
 * characters — whitespace for argv, newline/;/{/} for a statement — are the
 * caller's business and MUST only be honored while *quote == 0.
 *
 * Rules, in §6.1 order: (2) a bare quote opens/closes and is stripped;
 * (3) inside single quotes \' is a literal apostrophe and every other backslash
 * is literal, deliberately, so JSON payloads and text escape codes survive;
 * (4) inside double quotes there is no escape at all;
 * (5) a quoted run that is still open at end of input is an ERROR — the caller
 * reads *quote after its loop and reports it. Before 0.2.7 it silently ran to
 * end of input, which §6.1 itself named as the reason this whole bug class is
 * quiet: a mis-quoted argument swallowed the rest of the line (or, in a
 * transcript, the rest of the file) and dispatch still returned ok:true. */
int vc_quote_step(const char *p, char *quote, int *emit) {
  *emit = -1;
  char c = *p;
  if (!c) return 0;
  if (*quote) {
    if (*quote == '\'' && c == '\\' && p[1] == '\'') {
      *emit = '\'';
      return 2;
    }
    if (c == *quote) {
      *quote = 0;
      return 1;
    }
    *emit = (unsigned char)c;
    return 1;
  }
  if (c == '\'' || c == '"') {
    *quote = c;
    return 1;
  }
  *emit = (unsigned char)c;
  return 1;
}

/* Quote an arbitrary (NUL-free) byte string as ONE dispatcher argument, so that
 *     vc_argv_split(vc_arg_quote(v)) == [v]     for every v      (SPEC §6.1)
 * The trailing-backslash clause is not a nicety: without it a value ending in a
 * backslash puts that backslash immediately before the closing quote, rule 3
 * reads the pair as an escaped apostrophe, and the argument never closes — so
 * `C:\` silently becomes `C:'` and the rest of the line is eaten. Caller frees
 * with vc_free_str. */
char *vc_arg_quote(const char *value) {
  const char *v = value ? value : "";
  size_t n = strlen(v);
  size_t head = n; /* v_head = v minus its trailing run of backslashes */
  while (head > 0 && v[head - 1] == '\\') head--;
  size_t apos = 0;
  for (size_t i = 0; i < head; i++)
    if (v[i] == '\'') apos++;
  char *out = (char *)malloc(n + apos + 3);
  if (!out) return NULL;
  size_t o = 0;
  out[o++] = '\'';
  for (size_t i = 0; i < head; i++) {
    if (v[i] == '\'') out[o++] = '\\';
    out[o++] = v[i];
  }
  out[o++] = '\'';
  for (size_t i = head; i < n; i++) out[o++] = v[i]; /* trailing backslashes, bare */
  out[o] = 0;
  return out;
}

vc_argv vc_argv_split(const char *line) {
  vc_argv a;
  a.items = NULL;
  a.count = 0;
  a.unterminated = 0;
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
      /* rule 1: whitespace separates arguments — but only outside a quoted run */
      if (!quote && isspace((unsigned char)*p)) break;
      int emit;
      int used = vc_quote_step(p, &quote, &emit);
      if (used == 0) break;
      p += used;
      if (emit < 0) continue;
      if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }
      buf[n++] = (char)emit;
    }
    if (quote) a.unterminated = 1; /* rule 5 */
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

/* Is this element safe to emit bare — i.e. does it tokenize back to itself with
 * no quoting? Conservative on purpose: the character set below excludes every
 * byte that means something to the tokenizer (whitespace, quotes, backslash) AND
 * every byte that means something to Voidscript (`$ ( ) ; { } #`), so a joined
 * line is replayable both as a dispatcher command and as a script statement. */
static int arg_is_bare_safe(const char *s) {
  if (!*s) return 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p >= 'a' && *p <= 'z') continue;
    if (*p >= 'A' && *p <= 'Z') continue;
    if (*p >= '0' && *p <= '9') continue;
    if (strchr("_./:@+=,~^%-", *p)) continue;
    return 0;
  }
  return 1;
}

/* Join an argv back into a canonical, re-splittable command line: each element
 * quoted per §6.1 unless it is bare-safe, single-space separated. By the law
 * above this round-trips — vc_argv_split(vc_argv_join(a)) == a — which is what
 * lets Voidscript re-serialize a statement after interpolation instead of
 * re-scanning interpolated text as syntax (see scripts/voidscript.c), and what
 * makes the logged form of a command (SPEC §9) replayable. Caller frees. */
char *vc_argv_join(const vc_argv *a) {
  size_t cap = 1;
  char **q = (char **)calloc((size_t)(a->count > 0 ? a->count : 1), sizeof(char *));
  if (!q) return NULL;
  for (int i = 0; i < a->count; i++) {
    q[i] = arg_is_bare_safe(a->items[i]) ? dupstr(a->items[i])
                                        : vc_arg_quote(a->items[i]);
    cap += (q[i] ? strlen(q[i]) : 0) + 1;
  }
  char *out = (char *)malloc(cap);
  if (!out) {
    for (int i = 0; i < a->count; i++) free(q[i]);
    free(q);
    return NULL;
  }
  size_t o = 0;
  for (int i = 0; i < a->count; i++) {
    if (i) out[o++] = ' ';
    if (q[i]) {
      size_t L = strlen(q[i]);
      memcpy(out + o, q[i], L);
      o += L;
    }
    free(q[i]);
  }
  out[o] = 0;
  free(q);
  return out;
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
      {"rmdir", "mantle", "rm"},
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
