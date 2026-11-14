/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/logging/logging.h>

#define SLEEP_MS 200

/* The shallowest idle state, which every SoC has. SoCs name theirs in pbl/soc. */
#define STATE 0

static uint64_t prv_idle_us_while_sleeping(void) {
  uint64_t before_us = pbl_idle_residency_us(STATE);

  pbl_sleep_ms(SLEEP_MS);

  return pbl_idle_residency_us(STATE) - before_us;
}

int main(void) {
  uint64_t idle_us;

  /* With nothing to run, the CPU idles until the sleep is over. */
  idle_us = prv_idle_us_while_sleeping();
  PBL_LOG_INFO("main: idled for most of a %d ms sleep: %s", SLEEP_MS,
               idle_us > SLEEP_MS * 1000 / 2 ? "yes" : "no");

  /* A driver blocks a state its peripheral does not work in, while it is busy. Blocking the
   * shallowest one keeps the CPU running. */
  pbl_idle_state_block(STATE);
  idle_us = prv_idle_us_while_sleeping();
  pbl_idle_state_release(STATE);
  PBL_LOG_INFO("main: idled for any of a %d ms sleep with the state blocked: %s", SLEEP_MS,
               idle_us > 0 ? "yes" : "no");

  PBL_LOG_INFO("Sample done");

  return 0;
}
