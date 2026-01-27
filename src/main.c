#include "pc_types.h"
#include "pc_globals.h"
#include "pc_hist.h"
#include "pc_queue.h"
#include "pc_runinfo.h"
#include "pc_util.h"
#include "pc_workers.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static long parse_long(const char *s, int *ok) {
  errno = 0;
  char *end = NULL;
  long v = strtol(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0') {
    *ok = 0;
    return 0;
  }
  *ok = 1;
  return v;
}

static int parse_nonneg_ms_from_seconds(const char *s, unsigned int *out_ms) {
  int ok = 0;
  long sec = parse_long(s, &ok);
  if (!ok || sec < 0) return 0;
  if ((unsigned long)sec > (unsigned long)(UINT_MAX / 1000U)) return 0;
  *out_ms = (unsigned int)sec * 1000U;
  return 1;
}

static void usage(const char *prog) {
  fprintf(stderr,
    "Usage:\n"
    "  %s <producers 1-10> <consumers 1-3> <capacity 1-20> <timeout_s> [-d] [-s]\n"
    "     [--pmin <seconds>] [--pmax <seconds>] [--cmax <seconds>]\n"
    "Options:\n"
    "  -d  debug prints (default off)\n"
    "  -s  queue stats lines (default off)\n"
    "  --pmin <seconds>  minimum producer wait between writes\n"
    "  --pmax <seconds>  maximum producer wait between writes\n"
    "  --cmax <seconds>  maximum consumer wait between reads\n",
    prog);
}

int main(int argc, char **argv) {
  if (argc < 5) {
    usage(argv[0]);
    return 1;
  }

  int ok1, ok2, ok3, ok4;
  long p = parse_long(argv[1], &ok1);
  long c = parse_long(argv[2], &ok2);
  long cap = parse_long(argv[3], &ok3);
  long timeout_s = parse_long(argv[4], &ok4);

  if (!ok1 || !ok2 || !ok3 || !ok4) {
    fprintf(stderr, "Error: invalid numeric argument(s).\n");
    usage(argv[0]);
    return 1;
  }

  if (p < 1 || p > 10 || c < 1 || c > 3 || cap < 1 || cap > 20 || timeout_s < 1) {
    fprintf(stderr, "Error: argument out of allowed range.\n");
    usage(argv[0]);
    return 1;
  }

  pc_config_t cfg;
  cfg.prod_wait_min_ms = 0;
  cfg.prod_wait_max_ms = PRODUCER_MAX_SLEEP_MS;
  cfg.cons_wait_max_ms = CONSUMER_MAX_SLEEP_MS;

  for (int i = 5; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) g_debug = 1;
    else if (strcmp(argv[i], "-s") == 0) g_stats = 1;
    else if (strcmp(argv[i], "--pmin") == 0) {
      if (i + 1 >= argc || !parse_nonneg_ms_from_seconds(argv[i + 1], &cfg.prod_wait_min_ms)) {
        fprintf(stderr, "Error: invalid value for --pmin\n");
        usage(argv[0]);
        return 1;
      }
      i++;
    } else if (strcmp(argv[i], "--pmax") == 0) {
      if (i + 1 >= argc || !parse_nonneg_ms_from_seconds(argv[i + 1], &cfg.prod_wait_max_ms)) {
        fprintf(stderr, "Error: invalid value for --pmax\n");
        usage(argv[0]);
        return 1;
      }
      i++;
    } else if (strcmp(argv[i], "--cmax") == 0) {
      if (i + 1 >= argc || !parse_nonneg_ms_from_seconds(argv[i + 1], &cfg.cons_wait_max_ms)) {
        fprintf(stderr, "Error: invalid value for --cmax\n");
        usage(argv[0]);
        return 1;
      }
      i++;
    }
    else {
      fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
  }

  if (cfg.prod_wait_max_ms < cfg.prod_wait_min_ms) {
    fprintf(stderr, "Error: --pmax must be >= --pmin\n");
    usage(argv[0]);
    return 1;
  }

  hist_reset();
  print_run_header((int)p, (int)c, (int)cap, (int)timeout_s, &cfg);

  pq_t q;
  if (pq_init(&q, (int)cap) != 0) {
    fprintf(stderr, "Error: queue init failed.\n");
    return 1;
  }

  pthread_t *prod = calloc((size_t)p, sizeof(pthread_t));
  pthread_t *cons = calloc((size_t)c, sizeof(pthread_t));
  prod_arg_t *pargs = calloc((size_t)p, sizeof(prod_arg_t));
  cons_arg_t *cargs = calloc((size_t)c, sizeof(cons_arg_t));
  prod_stats_t *pstats = calloc((size_t)p, sizeof(prod_stats_t));
  cons_stats_t *cstats = calloc((size_t)c, sizeof(cons_stats_t));

  if (!prod || !cons || !pargs || !cargs || !pstats || !cstats) {
    fprintf(stderr, "Error: allocation failed.\n");
    pq_destroy(&q);
    free(prod); free(cons); free(pargs); free(cargs); free(pstats); free(cstats);
    return 1;
  }

#ifdef _WIN32
  uint64_t base_seed = (uint64_t)time(NULL) ^ (uint64_t)GetCurrentProcessId();
#else
  /* getpid declared in unistd.h, pulled in by pc_runinfo.c for POSIX */
  extern int getpid(void);
  uint64_t base_seed = (uint64_t)time(NULL) ^ (uint64_t)getpid();
#endif

  /* Start producers */
  for (int i = 0; i < (int)p; i++) {
    pargs[i].id = i;
    pargs[i].producer_priority = i % 10;
    pargs[i].q = &q;
    pargs[i].stats = &pstats[i];
    pargs[i].seed = (unsigned int)(base_seed + (uint64_t)(i * 101));
    pargs[i].cfg = &cfg;

    if (pthread_create(&prod[i], NULL, producer_thread, &pargs[i]) != 0) {
      fprintf(stderr, "Error: failed to create producer thread %d\n", i);
      g_stop = 1;
      break;
    }
  }

  /* Start consumers */
  for (int i = 0; i < (int)c; i++) {
    cargs[i].id = i;
    cargs[i].q = &q;
    cargs[i].stats = &cstats[i];
    cargs[i].seed = (unsigned int)(base_seed + 999 + (uint64_t)(i * 131));
    cargs[i].cfg = &cfg;

    if (pthread_create(&cons[i], NULL, consumer_thread, &cargs[i]) != 0) {
      fprintf(stderr, "Error: failed to create consumer thread %d\n", i);
      g_stop = 1;
      break;
    }
  }

  /* Once per second status */
  for (int elapsed = 0; elapsed < (int)timeout_s && !g_stop; elapsed++) {
    sleep_s(1);

    uint64_t total_prod_now = 0, total_cons_now = 0;
    uint64_t blocked_full_now = 0, blocked_empty_now = 0;
    int occ_now = 0, high_now = 0, low_now = 0;

    pthread_mutex_lock(&q.lock);
    occ_now = pq_count(&q);
    high_now = q.high_count;
    low_now = q.low_count;

    for (int i = 0; i < (int)p; i++) {
      total_prod_now += pstats[i].produced;
      blocked_full_now += pstats[i].blocked_full;
    }
    for (int i = 0; i < (int)c; i++) {
      total_cons_now += cstats[i].consumed;
      blocked_empty_now += cstats[i].blocked_empty;
    }
    pthread_mutex_unlock(&q.lock);

    printf("STATUS t=%ds produced=%llu consumed=%llu occ=%d high=%d low=%d blk_full=%llu blk_empty=%llu\n",
           elapsed + 1,
           (unsigned long long)total_prod_now,
           (unsigned long long)total_cons_now,
           occ_now, high_now, low_now,
           (unsigned long long)blocked_full_now,
           (unsigned long long)blocked_empty_now);
    fflush(stdout);
  }

  g_stop = 1;
  pq_wake_all(&q);

  for (int i = 0; i < (int)p; i++) pthread_join(prod[i], NULL);
  for (int i = 0; i < (int)c; i++) pthread_join(cons[i], NULL);

  /* Summary */
  uint64_t total_prod = 0, total_cons = 0, blocked_full = 0, blocked_empty = 0;
  uint64_t total_latency_ns = 0;

  for (int i = 0; i < (int)p; i++) {
    total_prod += pstats[i].produced;
    blocked_full += pstats[i].blocked_full;
  }
  for (int i = 0; i < (int)c; i++) {
    total_cons += cstats[i].consumed;
    blocked_empty += cstats[i].blocked_empty;
    total_latency_ns += cstats[i].latency_ns_total;
  }

  printf("\n=== END SUMMARY ===\n");
  printf("Total produced: %llu\n", (unsigned long long)total_prod);
  printf("Total consumed: %llu\n", (unsigned long long)total_cons);
  printf("Producer blocks (full): %llu\n", (unsigned long long)blocked_full);
  printf("Consumer blocks (empty): %llu\n", (unsigned long long)blocked_empty);

  if (total_cons > 0) {
    double avg_ms = (double)total_latency_ns / 1000000.0 / (double)total_cons;
    printf("Average latency: %.3f ms\n", avg_ms);
  } else {
    printf("Average latency: n/a\n");
  }

  printf("Final queue occupancy: %d (high=%d low=%d)\n",
         pq_count(&q), q.high_count, q.low_count);

  hist_print((int)cap);

  free(prod); free(cons); free(pargs); free(cargs); free(pstats); free(cstats);
  pq_destroy(&q);

  return 0;
}




