/* codec.c — the §6.1 command codec, exported.
 *
 * Void Hormiga's 2026-08-21 message put the argument well: "§6.1 is currently a
 * specification standing in for a component", and the evidence is that four
 * independent codebases implemented the spec wrong — including this one, twice,
 * in its own Voidscript reader. A rule that must be reimplemented is a rule that
 * will be reimplemented wrong. So the rule ships as code.
 *
 * The half nobody thinks to export is the DECODER. A host that reviews a
 * proposed transcript before dispatching it — Hormiga's visitor submissions,
 * Reyna's harvested datasets, Weaver's proposed rules — has to be able to ask
 * "what will this text actually do", with the very tokenizer that will do it.
 * vc_transcript_split_json is that question.
 *
 * Everything here is stateless and thread-safe (no VC_Manager), like
 * vc_tag_match. Results cross the boundary as JSON strings, per voidcore.h. */
#include "vc_internal.h"
#include <stdlib.h>
#include <string.h>

/* ---- vc_arg_quote is in args.c (it is also used internally) ---------------- */

static cJSON *argv_to_json(const vc_argv *a) {
  cJSON *arr = cJSON_CreateArray();
  for (int i = 0; i < a->count; i++)
    cJSON_AddItemToArray(arr, cJSON_CreateString(a->items[i]));
  return arr;
}

char *vc_argv_split_json(const char *line) {
  cJSON *res = cJSON_CreateObject();
  vc_argv a = vc_argv_split(line);
  if (a.unterminated) {
    cJSON_AddBoolToObject(res, "ok", 0);
    cJSON_AddStringToObject(res, "error",
                            "unterminated quote (SPEC \xc2\xa7""6.1 rule 5)");
    cJSON_AddItemToObject(res, "argv", cJSON_CreateNull());
  } else {
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddItemToObject(res, "argv", argv_to_json(&a));
  }
  vc_argv_free(&a);
  char *s = cJSON_PrintUnformatted(res);
  cJSON_Delete(res);
  return s;
}

/* The control-flow keywords of SPEC §8. A transcript containing none of them is
 * a flat list of dispatcher commands — which is what a reviewing host wants to
 * be told, because a flat transcript is one whose effect can be read off its
 * statements without simulating it. */
static int is_control_word(const char *w) {
  static const char *k[] = {"if",     "elif",  "else",   "while", "repeat",
                            "foreach", "break", "continue", "return", "halt",
                            "let",    "def",   "try",    "catch", "include",
                            "call",   "on",    "wait"};
  for (size_t i = 0; i < sizeof k / sizeof k[0]; i++)
    if (!strcmp(w, k[i])) return 1;
  return 0;
}

/* Split a transcript into the statements it will run: on newline and `;`, but
 * ONLY outside a quoted run (the whole point — a newline inside a value is data,
 * not a statement boundary), with `#` comments dropped. Each statement is
 * returned with the line it occupies, its raw text and its decoded argv, so a
 * gate can read argv[0] instead of guessing at the text.
 *
 *   {"ok":true,"flat":true,
 *    "commands":[{"line":4,"text":"set v bio 'a\nb'","argv":["set","v","bio","a\nb"]}]}
 *   {"ok":false,"error":"...","line":7}
 *
 * `flat` is false when any statement opens a block or begins with a SPEC §8
 * control word; block braces are reported as their own statements rather than
 * interpreted, because this is a splitter and not an interpreter. Caller frees
 * with vc_free_str. */
char *vc_transcript_split_json(const char *src) {
  cJSON *res = cJSON_CreateObject();
  cJSON *cmds = cJSON_CreateArray();
  const char *s = src ? src : "";
  int flat = 1;
  int line_no = 1;

  size_t bcap = 256, n = 0;
  char *buf = (char *)malloc(bcap);
  if (!buf) {
    cJSON_Delete(res);
    cJSON_Delete(cmds);
    return NULL;
  }
  char q = 0;
  int stmt_line = 1;
  int err_line = 0;
  const char *err = NULL;

#define TS_PUSH(ch)                                                            \
  do {                                                                         \
    if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }        \
    buf[n++] = (char)(ch);                                                     \
  } while (0)

  for (size_t i = 0; s[i] || n;) {
    int at_end = !s[i];
    /* A bare CR ends a statement exactly as LF does, so a CRLF transcript splits
     * the same as an LF one and no CR reaches a value (Void Unity, 2026-08-27).
     * Same rule as the Voidscript statement reader -- one answer, both readers. */
    int boundary =
        at_end || (!q && (s[i] == '\n' || s[i] == '\r' || s[i] == ';'));
    if (!boundary) {
      /* a comment runs to end of line — but only outside a quoted run */
      if (!q && s[i] == '#' && n == 0) {
        while (s[i] && s[i] != '\n') i++;
        continue;
      }
      if (!q && (s[i] == '{' || s[i] == '}')) flat = 0;
      if (n == 0 && (s[i] == ' ' || s[i] == '\t')) { i++; continue; }
      if (n == 0) stmt_line = line_no;
      int emit, used = vc_quote_step(s + i, &q, &emit);
      if (used == 0) break;
      for (int k = 0; k < used; k++) {
        /* A newline inside a quoted value is not a boundary, but it is still a
         * line. Counting only at boundaries left every statement after a
         * multi-line value low by that value's newline count -- and the same
         * counter feeds `err_line`, so an unterminated quote LATER in the file
         * was blamed on the wrong line. That is exactly the diagnostic this
         * function exists to give (Void Maiz, 2026-08-25). `stmt_line` is still
         * taken at the statement's first non-blank byte, so a multi-line
         * statement reports the line it STARTED on. */
        if (s[i + k] == '\n') line_no++;
        TS_PUSH(s[i + k]);
      }
      i += used;
      continue;
    }
    /* statement boundary */
    if (n + 1 >= bcap) { bcap *= 2; buf = (char *)realloc(buf, bcap); }
    buf[n] = 0;
    while (n > 0 && (buf[n - 1] == ' ' || buf[n - 1] == '\t')) buf[--n] = 0;
    if (n > 0) {
      vc_argv a = vc_argv_split(buf);
      if (a.unterminated) {
        err = "unterminated quote (SPEC \xc2\xa7""6.1 rule 5): this statement "
              "swallows the rest of the transcript";
        err_line = stmt_line;
        vc_argv_free(&a);
        break;
      }
      cJSON *c = cJSON_CreateObject();
      cJSON_AddNumberToObject(c, "line", stmt_line);
      cJSON_AddStringToObject(c, "text", buf);
      cJSON_AddItemToObject(c, "argv", argv_to_json(&a));
      if (a.count && is_control_word(a.items[0])) flat = 0;
      cJSON_AddItemToArray(cmds, c);
      vc_argv_free(&a);
    }
    n = 0;
    if (at_end) break;
    if (s[i] == '\n') line_no++;
    i++;
  }
#undef TS_PUSH
  free(buf);

  if (!err && q) {
    err = "unterminated quote (SPEC \xc2\xa7""6.1 rule 5): the transcript ends "
          "inside a quoted value";
    err_line = stmt_line;
  }
  if (err) {
    cJSON_Delete(cmds);
    cJSON_AddBoolToObject(res, "ok", 0);
    cJSON_AddStringToObject(res, "error", err);
    cJSON_AddNumberToObject(res, "line", err_line);
  } else {
    cJSON_AddBoolToObject(res, "ok", 1);
    cJSON_AddBoolToObject(res, "flat", flat);
    cJSON_AddItemToObject(res, "commands", cmds);
  }
  char *out = cJSON_PrintUnformatted(res);
  cJSON_Delete(res);
  return out;
}
