#include "pc_runinfo.h"
#include "pc_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static int safe_localtime(const time_t *t, struct tm *out) {
#ifdef _WIN32
  return localtime_s(out, t) == 0;
#else
  return localtime_r(t, out) != NULL;
#endif
}

void print_run_header(int p, int c, int cap, int timeout_s, const pc_config_t *cfg) {
  time_t t = time(NULL);
  struct tm tmv;

  if (!safe_localtime(&t, &tmv)) {
    struct tm *tmp = localtime(&t);
    if (tmp) tmv = *tmp;
    else memset(&tmv, 0, sizeof(tmv));
  }

  char tbuf[64];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tmv);

  char host[256];

#ifdef _WIN32
  DWORD host_len = (DWORD)sizeof(host);
  if (!GetComputerNameA(host, &host_len)) {
    strncpy(host, "unknown", sizeof(host));
    host[sizeof(host)-1] = '\0';
  } else {
    host[sizeof(host)-1] = '\0';
  }
#else
  if (gethostname(host, sizeof(host)) != 0) {
    strncpy(host, "unknown", sizeof(host));
    host[sizeof(host)-1] = '\0';
  } else {
    host[sizeof(host)-1] = '\0';
  }
#endif

  const char *user = "unknown";
#ifndef _WIN32
  struct passwd *pw = getpwuid(getuid());
  if (pw && pw->pw_name && pw->pw_name[0] != '\0') user = pw->pw_name;
#else
  const char *tmp_user = getenv("USERNAME");
  if (tmp_user && tmp_user[0] != '\0') user = tmp_user;
  else {
    tmp_user = getenv("USER");
    if (tmp_user && tmp_user[0] != '\0') user = tmp_user;
  }
#endif

  printf("=== RUN INFO ===\n");
  printf("Time/Date: %s\n", tbuf);
  printf("User: %s\n", user);
  printf("Host: %s\n", host);

  printf("\n=== RUNTIME PARAMS ===\n");
  printf("Producers: %d\nConsumers: %d\nCapacity: %d\nTimeout(s): %d\n",
         p, c, cap, timeout_s);
  if (cfg) {
    printf("Producer wait (runtime): %u..%u ms\n",
           cfg->prod_wait_min_ms, cfg->prod_wait_max_ms);
    printf("Consumer wait (runtime): 0..%u ms\n", cfg->cons_wait_max_ms);
  }

  printf("\n=== COMPILED DEFAULTS ===\n");
  printf("PRODUCER_MAX_SLEEP_MS=%d\nCONSUMER_MAX_SLEEP_MS=%d\n",
         PRODUCER_MAX_SLEEP_MS, CONSUMER_MAX_SLEEP_MS);
  printf("RAND_RANGE=%d..%d\nHIGH_PRIORITY_THRESHOLD=%d\n",
         RAND_RANGE_MIN, RAND_RANGE_MAX, HIGH_PRIORITY_THRESHOLD);
  printf("DEFAULT_PRODUCERS=%d DEFAULT_CONSUMERS=%d DEFAULT_CAPACITY=%d\n",
         DEFAULT_PRODUCERS, DEFAULT_CONSUMERS, DEFAULT_CAPACITY);

  printf("\n=== START ===\n");
}
