/* voidscript_internal.h — shared surface for the Voidscript interpreter, split
 * into vs_interp.c (variables + interpolation), vs_expr.c (the condition
 * expression evaluator), and voidscript.c (the statement parser/executor). */
#ifndef VC_VOIDSCRIPT_INTERNAL_H
#define VC_VOIDSCRIPT_INTERNAL_H
#include "vc_internal.h"

/* Interpreter context threaded through every layer. */
typedef struct {
  VC_Manager *m;
  cJSON *vars; /* name -> string */
  cJSON *args; /* $1.. */
  cJSON *out;  /* output lines */
  int ret, halt, halt_code, brk, cont, ok;
  long steps;
  char retval[1024];
} VS;

/* vs_interp.c — variable store + $var/$(...) interpolation. */
void vs_append(VS *c, const char *line);
void set_var(VS *c, const char *name, const char *val);
void interpolate(VS *c, const char *in, char *out, int outsz);
/* Interpolate into a fresh heap string (no length cap). When `maskp` is given it
 * also returns a parallel provenance mask: 0 = source syntax, 1 = expansion
 * content, 2 = zero-width field mark. Only the paths that re-tokenize the result
 * need the mask (see vs_command_line). Caller frees both. */
char *vs_interp_alloc(VS *c, const char *in, unsigned char **maskp);
/* Masked, interpolated statement text -> a canonical command line in which every
 * expansion is exactly one argument and can never become syntax. Caller frees. */
/* Decode masked, interpolated text as exactly one §6.1 argument (no field
 * splitting) — what `let x = <rest>` means. Caller frees. */
char *vs_decode_one(const char *t, const unsigned char *mask, int *unterminated);
char *vs_command_line(const char *t, const unsigned char *mask,
                      int *unterminated);

/* vs_expr.c — evaluate a condition string to a boolean (SPEC §8 operators). */
int eval_cond(VS *c, const char *text);

#endif
