/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Zephyr timeout and timepoint types for the PebbleOS port. One tick is one
// millisecond (CONFIG_SYS_CLOCK_TICKS_PER_SEC is 1000).

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/sys/time_units.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#define MSEC_PER_SEC  1000
#define USEC_PER_MSEC 1000
#define USEC_PER_SEC  1000000
#define NSEC_PER_USEC 1000
#define NSEC_PER_MSEC 1000000
#define NSEC_PER_SEC  1000000000

typedef int64_t k_ticks_t;

#define K_TICKS_FOREVER ((k_ticks_t)(-1))

typedef struct {
  k_ticks_t ticks;
} k_timeout_t;

#define Z_TIMEOUT_TICKS(t) ((k_timeout_t){.ticks = (t)})
#define Z_TIMEOUT_NO_WAIT  ((k_timeout_t){0})
#define Z_FOREVER          Z_TIMEOUT_TICKS(K_TICKS_FOREVER)
#define Z_TIMEOUT_MS(t)    Z_TIMEOUT_TICKS((k_ticks_t)k_ms_to_ticks_ceil64(MAX((t), 0)))
#define Z_TIMEOUT_US(t)    Z_TIMEOUT_TICKS((k_ticks_t)k_us_to_ticks_ceil64(MAX((t), 0)))
#define Z_TIMEOUT_NS(t)    Z_TIMEOUT_TICKS((k_ticks_t)k_ns_to_ticks_ceil64(MAX((t), 0)))
#define Z_TIMEOUT_CYC(t)   Z_TIMEOUT_TICKS((k_ticks_t)k_cyc_to_ticks_ceil64(MAX((t), 0)))
#define K_TIMEOUT_EQ(a, b) ((a).ticks == (b).ticks)
#define K_TICK_MIN         ((k_ticks_t)1)
#define K_TICK_MAX         ((k_ticks_t)INT64_MAX)

#define SYS_FOREVER_MS (-1)
#define SYS_FOREVER_US (-1)

typedef struct {
  uint64_t tick;
} k_timepoint_t;

k_timepoint_t sys_timepoint_calc(k_timeout_t timeout);
k_timeout_t sys_timepoint_timeout(k_timepoint_t timepoint);

static inline int sys_timepoint_cmp(k_timepoint_t a, k_timepoint_t b) {
  if (a.tick == b.tick) {
    return 0;
  }
  return a.tick < b.tick ? -1 : 1;
}

static inline bool sys_timepoint_expired(k_timepoint_t timepoint) {
  return K_TIMEOUT_EQ(sys_timepoint_timeout(timepoint), Z_TIMEOUT_NO_WAIT);
}
