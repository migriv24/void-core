/* dispatch_internal.h — shared surface for the dispatcher's verb-family modules.
 * The verb handlers live in verbs_*.c; this header wires them to the router
 * (dispatch.c) and to the shared result/helper utilities (dispatch_util.c). */
#ifndef VC_DISPATCH_INTERNAL_H
#define VC_DISPATCH_INTERNAL_H
#include "vc_internal.h"

#define VC_VERSION_STR "0.2.1"

extern const char *vc_facet_keys[6];

/* result builders + shared helpers (dispatch_util.c) */
cJSON *res_make(int ok);
void   res_line(cJSON *r, const char *fmt, ...);
void   res_set_data(cJSON *r, cJSON *data);
cJSON *res_fail(const char *fmt, ...);
const char *gstr(cJSON *o, const char *k);
cJSON *need_mantle(cJSON *state, cJSON **err);
int    collect_targets(cJSON *mt, const char *ref, cJSON ***out);
int    ci_contains(const char *hay, const char *needle);
void   set_near(cJSON *tags, const char *a, const char *b, double w);
void   del_near(cJSON *tags, const char *a, const char *b);

/* Verb-family handlers. Each returns a {ok,lines,data} result for the verb `v`,
 * or NULL if `v` is not in that family (so the router tries the next family). */
cJSON *vc_verbs_query(VC_Manager *m, cJSON *state, vc_argv a, const char *v);
cJSON *vc_verbs_edit(VC_Manager *m, cJSON *state, vc_argv a, const char *v);
cJSON *vc_verbs_graph(VC_Manager *m, cJSON *state, vc_argv a, const char *v);
cJSON *vc_verbs_lifecycle(VC_Manager *m, cJSON *state, vc_argv a, const char *v);
cJSON *vc_verbs_script(VC_Manager *m, cJSON *state, vc_argv a, const char *v);

#endif
