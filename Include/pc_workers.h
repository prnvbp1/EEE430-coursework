#pragma once
#include "pc_types.h"

typedef struct {
  int id;
  int producer_priority; // fixed 0..9 for this producer
  pq_t *q;
  prod_stats_t *stats;
  unsigned int seed;
  const pc_config_t *cfg;
} prod_arg_t;

typedef struct {
  int id;
  pq_t *q;
  cons_stats_t *stats;
  unsigned int seed;
  const pc_config_t *cfg;
} cons_arg_t;

void* producer_thread(void *arg);
void* consumer_thread(void *arg);
