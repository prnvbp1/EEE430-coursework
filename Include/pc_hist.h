#pragma once
#include <stdint.h>

/* Fixed-size occupancy histogram (0..63) */
void hist_reset(void);
void hist_record(int occupancy);
void hist_print(int cap);
