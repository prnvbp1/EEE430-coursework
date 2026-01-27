#pragma once
#include "pc_types.h"

int pq_init(pq_t *q, int cap);
void pq_destroy(pq_t *q);

int pq_count(const pq_t *q);

int pq_push(pq_t *q, const item_t *it, prod_stats_t *st);
/* If the queue contains >1 item, prefers the first high-priority entry (>= HIGH_PRIORITY_THRESHOLD),
   preserving stable FIFO order for the remaining elements. */
int pq_pop(pq_t *q, item_t *out, cons_stats_t *st);

/* Used during shutdown to wake blocked producers/consumers */
void pq_wake_all(pq_t *q);
