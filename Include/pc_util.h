#pragma once
#include <stdint.h>
#include <time.h>

uint64_t now_ms(void);

void ms_sleep(unsigned int ms);
void sleep_s(unsigned int seconds);

uint64_t timespec_to_ns(const struct timespec *ts);
uint64_t timespec_diff_ns(const struct timespec *end, const struct timespec *start);

unsigned int thread_rand(unsigned int *seed);
unsigned int rand_range_ms(unsigned int *seed, unsigned int min_ms, unsigned int max_ms);
