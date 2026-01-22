#include "pc_util.h"

#include <limits.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

uint64_t now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

void ms_sleep(unsigned int ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec req;
  req.tv_sec = (time_t)(ms / 1000);
  req.tv_nsec = (long)(ms % 1000) * 1000000L;
  nanosleep(&req, NULL);
#endif
}

void sleep_s(unsigned int seconds) {
#ifdef _WIN32
  Sleep(seconds * 1000U);
#else
  sleep(seconds);
#endif
}

uint64_t timespec_to_ns(const struct timespec *ts) {
  return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec;
}

uint64_t timespec_diff_ns(const struct timespec *end, const struct timespec *start) {
  uint64_t end_ns = timespec_to_ns(end);
  uint64_t start_ns = timespec_to_ns(start);
  return end_ns >= start_ns ? (end_ns - start_ns) : 0;
}

unsigned int thread_rand(unsigned int *seed) {
  unsigned int x = *seed;
  if (x == 0) x = 0xDEADBEEF;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *seed = x;
  return x;
}

unsigned int rand_range_ms(unsigned int *seed, unsigned int min_ms, unsigned int max_ms) {
  if (max_ms < min_ms) {
    unsigned int tmp = min_ms;
    min_ms = max_ms;
    max_ms = tmp;
  }

  if (min_ms == max_ms) return min_ms;

  uint64_t range = (uint64_t)max_ms - (uint64_t)min_ms + 1ULL;
  uint64_t bound = (uint64_t)UINT_MAX + 1ULL;
  uint64_t limit = bound - (bound % range);

  unsigned int r;
  do {
    r = thread_rand(seed);
  } while ((uint64_t)r >= limit);

  return min_ms + (unsigned int)((uint64_t)r % range);
}
