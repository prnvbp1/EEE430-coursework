#pragma once

#ifndef _WIN32
/* Ensure POSIX prototypes are visible when building on POSIX */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>

/* Defaults / config */
#ifndef DEFAULT_PRODUCERS
#define DEFAULT_PRODUCERS 2
#endif

#ifndef DEFAULT_CONSUMERS
#define DEFAULT_CONSUMERS 1
#endif

#ifndef DEFAULT_CAPACITY
#define DEFAULT_CAPACITY 8
#endif

#ifndef PRODUCER_MAX_SLEEP_MS
#define PRODUCER_MAX_SLEEP_MS 2000
#endif

#ifndef CONSUMER_MAX_SLEEP_MS
#define CONSUMER_MAX_SLEEP_MS 4000
#endif

#ifndef RAND_RANGE_MIN
#define RAND_RANGE_MIN 0
#endif

#ifndef RAND_RANGE_MAX
#define RAND_RANGE_MAX 9
#endif

#ifndef HIGH_PRIORITY_THRESHOLD
#define HIGH_PRIORITY_THRESHOLD 7
#endif

typedef struct {
  int value;
  int priority;        // 0..9
  int producer_id;
  uint64_t seq;
  struct timespec ts;  // enqueue timestamp (monotonic clock)
} item_t;

typedef struct {
  /* Single ring buffer (FIFO) */
  item_t *buf;
  int cap;

  int head, tail, count;

  pthread_mutex_t lock;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;
} pq_t;

typedef struct {
  uint64_t produced;
  uint64_t blocked_full;
} prod_stats_t;

typedef struct {
  uint64_t consumed;
  uint64_t blocked_empty;
  uint64_t latency_ns_total;
} cons_stats_t;

typedef struct {
  unsigned int prod_wait_min_ms;
  unsigned int prod_wait_max_ms;
  unsigned int cons_wait_max_ms;
} pc_config_t;
