#include "pc_queue.h"
#include "pc_globals.h"
#include "pc_hist.h"
#include "pc_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pq_count(const pq_t *q) {
  return q->count;
}

static int ring_idx(const pq_t *q, int offset) {
  return (q->head + offset) % q->cap;
}

int pq_init(pq_t *q, int cap) {
  memset(q, 0, sizeof(*q));
  q->cap = cap;

  q->buf = (item_t*)calloc((size_t)cap, sizeof(item_t));
  if (!q->buf) return -1;

  if (pthread_mutex_init(&q->lock, NULL) != 0) {
    free(q->buf);
    q->buf = NULL;
    return -1;
  }
  if (pthread_cond_init(&q->not_full, NULL) != 0) {
    pthread_mutex_destroy(&q->lock);
    free(q->buf);
    q->buf = NULL;
    return -1;
  }
  if (pthread_cond_init(&q->not_empty, NULL) != 0) {
    pthread_cond_destroy(&q->not_full);
    pthread_mutex_destroy(&q->lock);
    free(q->buf);
    q->buf = NULL;
    return -1;
  }

  return 0;
}

void pq_destroy(pq_t *q) {
  pthread_mutex_destroy(&q->lock);
  pthread_cond_destroy(&q->not_full);
  pthread_cond_destroy(&q->not_empty);
  free(q->buf);
  q->buf = NULL;
}

int pq_push(pq_t *q, const item_t *it, prod_stats_t *st) {
  pthread_mutex_lock(&q->lock);
  while (!g_stop && q->count >= q->cap) {
    st->blocked_full++;
    pthread_cond_wait(&q->not_full, &q->lock);
  }
  if (g_stop) {
    pthread_mutex_unlock(&q->lock);
    return 0;
  }

  q->buf[q->tail] = *it;
  q->tail = (q->tail + 1) % q->cap;
  q->count++;

  st->produced++;

  int occ = q->count;
  if (g_stats) {
    printf("QSTAT t_ms=%llu occ=%d\n", (unsigned long long)now_ms(), occ);
  }
  hist_record(occ);

  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->lock);
  return 1;
}

int pq_pop(pq_t *q, item_t *out, cons_stats_t *st) {
  pthread_mutex_lock(&q->lock);
  while (!g_stop && q->count == 0) {
    st->blocked_empty++;
    pthread_cond_wait(&q->not_empty, &q->lock);
  }
  if (g_stop && q->count == 0) {
    pthread_mutex_unlock(&q->lock);
    return 0;
  }

  if (q->count > 1) {
    int high_offset = -1;
    for (int off = 0; off < q->count; off++) {
      int i = ring_idx(q, off);
      if (q->buf[i].priority >= HIGH_PRIORITY_THRESHOLD) {
        high_offset = off;
        break;
      }
    }

    if (high_offset >= 0) {
      /* Remove the first high-priority element while keeping the remaining order stable.
         We do this by shifting elements between head..(head+high_offset-1) "forward" by one
         slot in the ring, overwriting the removed slot, then advancing head by 1. */
      *out = q->buf[ring_idx(q, high_offset)];
      for (int j = high_offset; j > 0; --j) {
        q->buf[ring_idx(q, j)] = q->buf[ring_idx(q, j - 1)];
      }
      q->head = (q->head + 1) % q->cap;
      q->count--;
    } else {
      *out = q->buf[q->head];
      q->head = (q->head + 1) % q->cap;
      q->count--;
    }
  } else {
    *out = q->buf[q->head];
    q->head = (q->head + 1) % q->cap;
    q->count--;
  }

  st->consumed++;

  int occ = q->count;
  if (g_stats) {
    printf("QSTAT t_ms=%llu occ=%d\n", (unsigned long long)now_ms(), occ);
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
