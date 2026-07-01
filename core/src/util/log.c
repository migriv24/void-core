/* log.c — the logging spine (SPEC §9). The core keeps an in-memory ring of log
 * records and, if the host registered a sink, streams each line out live. The
 * core never writes a file itself — where logs go is the host's job (a holiday).
 */
#include "vc_internal.h"
#include <stdarg.h>
#include <stdio.h>
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
  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);

  char ts[32];
  iso_now(ts, sizeof ts);

  cJSON *rec = cJSON_CreateObject();
  cJSON_AddStringToObject(rec, "ts", ts);
  cJSON_AddStringToObject(rec, "level", level ? level : "INFO");
  cJSON_AddStringToObject(rec, "op", op ? op : "");
  cJSON_AddStringToObject(rec, "msg", msg);

  cJSON *buf = vc_log_buffer(m);
  cJSON_AddItemToArray(buf, rec);
  while (cJSON_GetArraySize(buf) > VC_LOG_MAX) cJSON_DeleteItemFromArray(buf, 0);

  if (m->log_sink) m->log_sink(level ? level : "INFO", op ? op : "", msg, m->log_user);
}
