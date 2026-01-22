#include "pc_hist.h"
#include <pthread.h>
#include <stdio.h>

static uint64_t g_q_hist[64];
static pthread_mutex_t g_hist_lock = PTHREAD_MUTEX_INITIALIZER;

void hist_reset(void) {
  pthread_mutex_lock(&g_hist_lock);
  for (int i = 0; i < 64; i++) g_q_hist[i] = 0;
  pthread_mutex_unlock(&g_hist_lock);
}

void hist_record(int occupancy) {
  if (occupancy < 0) return;
  if (occupancy >= 64) return;
  pthread_mutex_lock(&g_hist_lock);
  g_q_hist[occupancy]++;
  pthread_mutex_unlock(&g_hist_lock);
}

void hist_print(int cap) {
  if (cap < 0) cap = 0;
  if (cap > 63) cap = 63;

  printf("\nQueue occupancy histogram (occ : count):\n");
  pthread_mutex_lock(&g_hist_lock);
  for (int i = 0; i <= cap; i++) {
    if (g_q_hist[i] > 0) {
      printf("%2d : %llu\n", i, (unsigned long long)g_q_hist[i]);
    }
  }
  pthread_mutex_unlock(&g_hist_lock);
}
