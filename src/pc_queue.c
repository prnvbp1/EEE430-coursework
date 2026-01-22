#include "pc_queue.h"
#include "pc_globals.h"
#include "pc_hist.h"
#include "pc_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pq_count(const pq_t *q) {
  return q->high_count + q->low_count;
}

int pq_init(pq_t *q, int cap) {
  memset(q, 0, sizeof(*q));
  q->cap = cap;

  q->high = (item_t*)calloc((size_t)cap, sizeof(item_t));
  q->low  = (item_t*)calloc((size_t)cap, sizeof(item_t));
  if (!q->high || !q->low) return -1;

  if (pthread_mutex_init(&q->lock, NULL) != 0) return -1;
  if (pthread_cond_init(&q->not_full, NULL) != 0) return -1;
  if (pthread_cond_init(&q->not_empty, NULL) != 0) return -1;

  return 0;
}

void pq_destroy(pq_t *q) {
  pthread_mutex_destroy(&q->lock);
  pthread_cond_destroy(&q->not_full);
  pthread_cond_destroy(&q->not_empty);
  free(q->high);
  free(q->low);
}

int pq_push(pq_t *q, const item_t *it, prod_stats_t *st) {
  int is_high = (it->priority >= HIGH_PRIORITY_THRESHOLD);

  pthread_mutex_lock(&q->lock);
  while (!g_stop && pq_count(q) >= q->cap) {
    st->blocked_full++;
    pthread_cond_wait(&q->not_full, &q->lock);
  }
  if (g_stop) {
    pthread_mutex_unlock(&q->lock);
    return 0;
  }

  if (is_high) {
    q->high[q->high_tail] = *it;
    q->high_tail = (q->high_tail + 1) % q->cap;
    q->high_count++;
  } else {
    q->low[q->low_tail] = *it;
    q->low_tail = (q->low_tail + 1) % q->cap;
    q->low_count++;
  }

  st->produced++;

  int occ = pq_count(q);
  if (g_stats) {
    printf("QSTAT t_ms=%llu occ=%d high=%d low=%d\n",
           (unsigned long long)now_ms(), occ, q->high_count, q->low_count);
  }
  hist_record(occ);

  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->lock);
  return 1;
}

int pq_pop(pq_t *q, item_t *out, cons_stats_t *st) {
  pthread_mutex_lock(&q->lock);
  while (!g_stop && pq_count(q) == 0) {
    st->blocked_empty++;
    pthread_cond_wait(&q->not_empty, &q->lock);
  }
  if (g_stop && pq_count(q) == 0) {
    pthread_mutex_unlock(&q->lock);
    return 0;
  }

  if (q->high_count > 0) {
    *out = q->high[q->high_head];
    q->high_head = (q->high_head + 1) % q->cap;
    q->high_count--;
  } else {
    *out = q->low[q->low_head];
    q->low_head = (q->low_head + 1) % q->cap;
    q->low_count--;
  }

  st->consumed++;

  int occ = pq_count(q);
  if (g_stats) {
    printf("QSTAT t_ms=%llu occ=%d high=%d low=%d\n",
           (unsigned long long)now_ms(), occ, q->high_count, q->low_count);
  }
  hist_record(occ);

  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->lock);
  return 1;
}

void pq_wake_all(pq_t *q) {
  pthread_mutex_lock(&q->lock);
  pthread_cond_broadcast(&q->not_empty);
  pthread_cond_broadcast(&q->not_full);
  pthread_mutex_unlock(&q->lock);
}
