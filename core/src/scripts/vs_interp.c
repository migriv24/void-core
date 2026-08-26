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

/* ── variable / capture interpolation ──────────────────────────────────────
 *
 * Two rules here are load-bearing for safety, and both were absent before
 * 0.2.7 (see SPEC §8, "Quoting and expansion"):
 *
 *  1. `$` does NOT expand inside a single-quoted run. §6.1 rule 3 already
 *     promises single quotes carry arbitrary text untouched; `$` is no
 *     exception. Without this, a transcript built by correctly quoting a
 *     stranger's text — which is exactly what a submission/proposal flow does —
 *     runs whatever `$(...)` that stranger wrote. Double quotes and bare words
 *     still expand, as they always did.
 *
 *  2. Bytes produced BY an expansion are masked, and the command path (see
 *     voidscript.c) treats masked bytes as pure literal content: never a
 *     separator, never a quote, never a flag. Without this, a variable holding
 *     `two words` silently became two arguments, one holding `a'b` lost its
 *     apostrophe, one holding `x --json` grew a flag, and an empty one vanished
 *     entirely — 5/5 hostile values corrupted, ok:true. An expanded value is now
 *     always exactly one argument, whatever is in it.
 *
 * `mask` may be NULL when the caller does not re-tokenize the result (let,
 * conditions, echo). */
/* mask codes: 0 = source syntax (scanned normally), 1 = expansion content
 * (pure literal), 2 = VS_FIELD_MARK, a zero-width "an expansion happened here"
 * sentinel that starts a field so an EMPTY expansion still yields an explicit
 * empty argument instead of vanishing. The sentinel byte is only written when a
 * mask is present, i.e. only on the command path that consumes it. */
#define VS_FIELD_MARK 0x01
/* A growable text+mask sink. Statement and interpolation buffers used to be
 * fixed 2 KB stack arrays; a value longer than that was cut mid-character, which
 * (a) silently truncated ordinary multi-line content — a volunteer typing two
 * paragraphs is over the line in an accented language — and (b) produced invalid
 * UTF-8. There is no length cap here, matching the tokenizer's own promise. */
typedef struct {
  char *out;
  unsigned char *mask; /* NULL when the caller does not re-tokenize */
  size_t n, cap;
  int want_mask;
} VSBuf;

static void vsbuf_init(VSBuf *b, int want_mask) {
  b->cap = 256;
  b->n = 0;
  b->want_mask = want_mask;
  b->out = (char *)malloc(b->cap);
  b->mask = want_mask ? (unsigned char *)malloc(b->cap) : NULL;
  if (b->out) b->out[0] = 0;
}
static void vsbuf_grow(VSBuf *b, size_t need) {
  if (b->n + need + 1 <= b->cap) return;
  while (b->n + need + 1 > b->cap) b->cap *= 2;
  b->out = (char *)realloc(b->out, b->cap);
  if (b->want_mask) b->mask = (unsigned char *)realloc(b->mask, b->cap);
}
static void put_ch(VSBuf *b, char ch, int mask_val) {
  vsbuf_grow(b, 1);
  if (!b->out) return;
  if (b->mask) b->mask[b->n] = (unsigned char)mask_val;
  b->out[b->n++] = ch;
  b->out[b->n] = 0;
}
static void put_str(VSBuf *b, const char *s, int mask_val) {
  for (int k = 0; s && s[k]; k++) put_ch(b, s[k], mask_val);
}
/* mark the start of an expansion's output (no-op when the caller has no mask) */
static void put_field_mark(VSBuf *b) {
  if (b->mask) put_ch(b, VS_FIELD_MARK, 2);
}

static void interp_capture(VS *c, const char *cmd, VSBuf *b) {
  unsigned char *im = NULL;
  char *ic = vs_interp_alloc(c, cmd, &im);
  free(im);
  cJSON *r = vc_dispatch_json(c->m, ic ? ic : "");
  put_field_mark(b);
  if (ic && strstr(ic, "--json")) {
    cJSON *d = cJSON_GetObjectItemCaseSensitive(r, "data");
    if (cJSON_IsString(d)) {
      /* A string `data` captures as ITSELF, not as its JSON encoding. Printing
       * it as JSON wrapped it in quotes and escaped its contents, and the code
       * downstream then un-wrapped it by accident (`let` stripped a surrounding
       * quote pair; the condition lexer treated a leading `"` as a quoted
       * token). That accident was also lossy: a captured value containing a
       * newline came back as the two characters \n and stayed that way. */
      put_str(b, d->valuestring, 1);
    } else {
      char *s = cJSON_PrintUnformatted(d);
      put_str(b, s, 1);
      free(s);
    }
  } else {
    cJSON *ln = NULL;
    int first = 1;
    cJSON_ArrayForEach(ln, cJSON_GetObjectItemCaseSensitive(r, "lines")) {
      if (!cJSON_IsString(ln)) continue;
      if (!first) put_ch(b, ' ', 1);
      put_str(b, ln->valuestring, 1);
      first = 0;
    }
  }
  cJSON_Delete(r);
  free(ic);
}

/* Interpolate `in`, returning a malloc'd string; when `maskp` is non-NULL, also
 * returns a malloc'd parallel provenance mask (0 = source syntax, 1 = expansion
 * content, 2 = zero-width field mark). Caller frees both. No length cap. */
char *vs_interp_alloc(VS *c, const char *in, unsigned char **maskp) {
  VSBuf b;
  vsbuf_init(&b, maskp != NULL);
  char q = 0; /* §6.1 quote state, via the ONE automaton in dispatch/args.c */
  for (int i = 0; in && in[i];) {
    /* rule 1 above: single quotes suppress expansion entirely */
    if (in[i] != '$' || q == '\'') {
      int emit, used = vc_quote_step(in + i, &q, &emit);
      if (used == 0) break;
      for (int k = 0; k < used; k++) put_ch(&b, in[i + k], 0);
      i += used;
      continue;
    }
    if (in[i + 1] == '(') { /* $(command) capture */
      int j = i + 2, d = 1;
      size_t ccap = 128, cn = 0;
      char *cmd = (char *)malloc(ccap);
      while (in[j] && d > 0) {
        if (in[j] == '(') d++;
        else if (in[j] == ')') { d--; if (d == 0) break; }
        if (cn + 1 >= ccap) { ccap *= 2; cmd = (char *)realloc(cmd, ccap); }
        cmd[cn++] = in[j];
        j++;
      }
      cmd[cn] = 0;
      interp_capture(c, cmd, &b);
      free(cmd);
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

    put_field_mark(&b);
    if (!strcmp(name, "@")) { /* all args, space-joined */
      cJSON *el = NULL;
      int first = 1;
      cJSON_ArrayForEach(el, c->args) {
        if (!first) put_ch(&b, ' ', 1);
        if (cJSON_IsString(el)) put_str(&b, el->valuestring, 1);
        first = 0;
      }
    } else if (!strcmp(name, "?")) {
      char nb[16];
      snprintf(nb, sizeof nb, "%d", c->ok);
      put_str(&b, nb, 1);
    } else {
      const char *val = NULL;
      if (name[0] >= '0' && name[0] <= '9') {
        cJSON *el = cJSON_GetArrayItem(c->args, atoi(name) - 1);
        val = cJSON_IsString(el) ? el->valuestring : "";
      } else {
        val = get_var(c, name);
      }
      /* An expansion always produces a field, even when empty — the field mark
       * above turns "" into an explicit empty argument rather than dropping it
       * (POSIX drops it; dropping it here was silent data loss). */
      if (val) put_str(&b, val, 1);
    }
    i = j;
  }
  if (maskp) *maskp = b.mask;
  return b.out;
}

void interpolate(VS *c, const char *in, char *out, int outsz) {
  char *s = vs_interp_alloc(c, in, NULL);
  snprintf(out, (size_t)outsz, "%s", s ? s : "");
  free(s);
}

/* Decode masked, interpolated text as exactly ONE §6.1 argument — no field
 * splitting, because the caller (`let x = <rest>`) means "all of the rest is the
 * value". Replaces a hand-rolled "strip one surrounding quote pair", which got
 * `'don\'t'` wrong (it left the backslash) and could not see quotes anywhere but
 * the ends. Caller frees. */
char *vs_decode_one(const char *t, const unsigned char *mask, int *unterminated) {
  size_t bcap = 64, n = 0;
  char *b = (char *)malloc(bcap);
  if (!b) return NULL;
  char q = 0;
  if (unterminated) *unterminated = 0;
  for (int i = 0; t[i];) {
    unsigned char mv = mask ? mask[i] : 0;
    if (mv) {
      if (mv == 1) {
        if (n + 1 >= bcap) { bcap *= 2; b = (char *)realloc(b, bcap); }
        b[n++] = t[i];
      }
      i++;
      continue;
    }
    int emit, used = vc_quote_step(t + i, &q, &emit);
    if (used == 0) break;
    i += used;
    if (emit < 0) continue;
    if (n + 1 >= bcap) { bcap *= 2; b = (char *)realloc(b, bcap); }
    b[n++] = (char)emit;
  }
  if (n + 1 >= bcap) { bcap *= 2; b = (char *)realloc(b, bcap); }
  b[n] = 0;
  if (q && unterminated) *unterminated = 1;
  return b;
}

char *vs_command_line(const char *t, const unsigned char *mask, int *unterminated) {
  vc_argv a;
  a.items = NULL;
  a.count = 0;
  a.unterminated = 0;
  if (unterminated) *unterminated = 0;
  int cap = 0;
  size_t bcap = 256, n = 0;
  char *buf = (char *)malloc(bcap);
  if (!buf) return NULL;
  char q = 0;
  int in_field = 0;
#define VS_PUSH(ch)                                                            \
  do {                                                                         \
    if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }        \
    buf[n++] = (char)(ch);                                                     \
  } while (0)
#define VS_FLUSH()                                                             \
  do {                                                                         \
    if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }        \
    buf[n] = 0;                                                                \
    if (a.count >= cap) {                                                      \
      cap = cap ? cap * 2 : 8;                                                 \
      a.items = (char **)realloc(a.items, (size_t)cap * sizeof(char *));       \
    }                                                                          \
    a.items[a.count] = (char *)malloc(n + 1);                                  \
    memcpy(a.items[a.count], buf, n + 1);                                      \
    a.count++;                                                                 \
    n = 0;                                                                     \
    in_field = 0;                                                              \
  } while (0)

  for (int i = 0; t[i];) {
    unsigned char mv = mask ? mask[i] : 0;
    if (mv) { /* expansion output — literal content, or a zero-width field mark */
      in_field = 1;
      if (mv == 1) VS_PUSH(t[i]);
      i++;
      continue;
    }
    if (!q && isspace((unsigned char)t[i])) {
      if (in_field) VS_FLUSH();
      i++;
      continue;
    }
    in_field = 1;
    int emit, used = vc_quote_step(t + i, &q, &emit);
    if (used == 0) break;
    i += used;
    if (emit >= 0) VS_PUSH(emit);
  }
  if (in_field) VS_FLUSH();
#undef VS_PUSH
#undef VS_FLUSH
  free(buf);
  /* §6.1 rule 5 at the statement level: a quoted run still open at the end of a
   * statement has swallowed the rest of the transcript. Loud, not silent. */
  if (q && unterminated) *unterminated = 1;
  char *line = vc_argv_join(&a);
  vc_argv_free(&a);
  return line;
}

/* ── expression evaluation (for conditions) ──────────────────────────────── */
