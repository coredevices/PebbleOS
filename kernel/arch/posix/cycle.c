/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <time.h>

#include <pbl_arch_cycle.h>

uint32_t arch_cycle_get_32(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)((uint64_t)ts.tv_sec * 1000000000U + (uint64_t)ts.tv_nsec);
}
