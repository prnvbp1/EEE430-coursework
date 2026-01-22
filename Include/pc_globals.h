#pragma once
#include <signal.h>

/* Global runtime flags shared across modules */
extern volatile sig_atomic_t g_stop;
extern int g_debug;
extern int g_stats;
