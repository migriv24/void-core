/* log.c — the logging spine (SPEC §9). The core keeps an in-memory ring of log
 * records and, if the host registered a sink, streams each line out live. The
 * core never writes a file itself — where logs go is the host's job (a holiday).
 */
#include "vc_internal.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define VC_LOG_MAX 2000

static void iso_now(char *buf, size_t n) {
  time_t t = time(NULL);
  struct tm tmv;
#if defined(_WIN32)
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  strftime(buf, n, "%Y-%m-%dT%H:%M:%S", &tmv);
}

cJSON *vc_log_buffer(VC_Manager *m) {
  if (!m->log) m->log = cJSON_CreateArray();
  return m->log;
}

void vc_log(VC_Manager *m, const char *level, const char *op, const char *fmt, ...) {
  if (!m) return;
  /* No length cap: the mutation spine (SPEC §9) logs the FULL command, and a
   * command carrying a long value used to be cut at 1024 bytes — mid-UTF-8
   * sequence for any non-ASCII text, which put invalid bytes into the log
   * record and therefore into the host's JSON. */
  va_list ap;
  va_start(ap, fmt);
  va_list cp;
  va_copy(cp, ap);
  int need = vsnprintf(NULL, 0, fmt, cp);
  va_end(cp);
  char *msg = (need >= 0) ? (char *)malloc((size_t)need + 1) : NULL;
  if (msg) vsnprintf(msg, (size_t)need + 1, fmt, ap);
  va_end(ap);

  char ts[32];
  iso_now(ts, sizeof ts);

  cJSON *rec = cJSON_CreateObject();
  cJSON_AddStringToObject(rec, "ts", ts);
  cJSON_AddStringToObject(rec, "level", level ? level : "INFO");
  cJSON_AddStringToObject(rec, "op", op ? op : "");
  cJSON_AddStringToObject(rec, "msg", msg ? msg : "");
  /* attribution (SPEC §9): stamp the session actor when one is configured, so
   * agents and humans sharing one dispatcher seam stay distinguishable */
  const char *who = m->state ? vc_actor(m->state) : NULL;
  if (who) cJSON_AddStringToObject(rec, "who", who);

  cJSON *buf = vc_log_buffer(m);
  cJSON_AddItemToArray(buf, rec);
  while (cJSON_GetArraySize(buf) > VC_LOG_MAX) cJSON_DeleteItemFromArray(buf, 0);

  if (m->log_sink)
    m->log_sink(level ? level : "INFO", op ? op : "", msg ? msg : "", m->log_user);
  free(msg);
}
