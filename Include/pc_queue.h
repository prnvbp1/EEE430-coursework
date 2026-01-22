#pragma once
#include "pc_types.h"

int pq_init(pq_t *q, int cap);
void pq_destroy(pq_t *q);

int pq_count(const pq_t *q);

int pq_push(pq_t *q, const item_t *it, prod_stats_t *st);
int pq_pop(pq_t *q, item_t *out, cons_stats_t *st);

/* Used during shutdown to wake blocked producers/consumers */
void pq_wake_all(pq_t *q);
