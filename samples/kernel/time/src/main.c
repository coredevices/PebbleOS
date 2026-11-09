/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>

/* 2026-01-01 00:00:00 UTC */
#define NEW_YEAR 1767225600

static bool prv_device_ready(void) {
  static unsigned int s_polls;

  return ++s_polls >= 3;
}

int main(void) {
  uint64_t start_ms;
  int ret;

  /* Uptime only moves forward: use it to measure and to time out. */
  start_ms = pbl_uptime_ms();
  pbl_sleep_ms(50);
  PBL_LOG_INFO("time: slept 50 ms, uptime moved by at least 50 ms: %s",
               pbl_uptime_ms() - start_ms >= 50 ? "yes" : "no");

  /* The wall clock follows the uptime, and jumps when it is set. */
  pbl_realtime_set(NEW_YEAR);
  pbl_sleep_ms(1000);
  PBL_LOG_INFO("time: one second after it was set, the wall clock moved by %" PRId32 " s",
               (int32_t)(pbl_realtime() - NEW_YEAR));

  /* Waiting for a condition, with the time limit every such wait needs. */
  ret = PBL_POLL_SLEEP_MS(prv_device_ready(), 5, 100);
  PBL_LOG_INFO("time: device ready: %s", ret == 0 ? "yes" : "no");

  ret = PBL_POLL_SLEEP_MS(false, 5, 20);
  PBL_LOG_INFO("time: a condition that never holds returns %s",
               ret == -ETIMEDOUT ? "-ETIMEDOUT" : "something else");

  PBL_LOG_INFO("Sample done");

  return 0;
}
