#include "pc_workers.h"
#include "pc_globals.h"
#include "pc_queue.h"
#include "pc_util.h"

#include <stdio.h>

void* producer_thread(void *arg) {
  prod_arg_t *a = (prod_arg_t*)arg;
  uint64_t seq = 0;

  while (!g_stop) {
    item_t it;
    it.value = RAND_RANGE_MIN + (int)(thread_rand(&a->seed) % (unsigned)(RAND_RANGE_MAX - RAND_RANGE_MIN + 1));
    it.priority = a->producer_priority;
    it.producer_id = a->id;
    it.seq = seq++;
    clock_gettime(CLOCK_MONOTONIC, &it.ts);

    int ok = pq_push(a->q, &it, a->stats);
    if (!ok) break;

    if (g_debug) {
      printf("DBG Producer[%d] wrote value=%d prio=%d seq=%llu\n",
             a->id, it.value, it.priority, (unsigned long long)it.seq);
    }

    unsigned int nap = rand_range_ms(&a->seed, a->cfg->prod_wait_min_ms, a->cfg->prod_wait_max_ms);
    ms_sleep(nap);
  }
  return NULL;
}

void* consumer_thread(void *arg) {
  cons_arg_t *a = (cons_arg_t*)arg;

  while (!g_stop) {
    item_t it;
    int ok = pq_pop(a->q, &it, a->stats);
    if (!ok) break;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    a->stats->latency_ns_total += timespec_diff_ns(&now, &it.ts);

    printf("Consumer[%d] got value=%d prio=%d from Producer[%d] seq=%llu\n",
           a->id, it.value, it.priority, it.producer_id, (unsigned long long)it.seq);

    unsigned int nap = rand_range_ms(&a->seed, 0, a->cfg->cons_wait_max_ms);
    ms_sleep(nap);
  }
  return NULL;
}
