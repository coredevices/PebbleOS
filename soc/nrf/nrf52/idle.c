/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>
#include <pbl/kernel/idle.h>
#include <pbl/soc/nrf/sleep.h>
#include <pbl/util/size.h>

#include <cmsis_core.h>
#include <kernel/util/idle.h>

enum {
  IDLE_WFI,
  IDLE_FULL,
};

static void prv_wfi(void) {
  __DSB();
  __WFI();
  __ISB();
}

static void prv_wfi_cb(void *arg) {
  prv_wfi();
}

// WFI with the external flash powered down.
static void prv_full_sleep(void) {
  pbl_flash_power_down(FLASH, prv_wfi_cb, nullptr);
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  [IDLE_WFI] = {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_wfi},
  [IDLE_FULL] = {.name = "full", .min_ticks = 5, .exit_ticks = 2, .enter = prv_full_sleep},
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

int pbl_soc_idle_state_limit(void) {
  if (!idle_is_allowed()) {
    return -1;
  }

  return soc_nrf_sleep_full_is_allowed() ? IDLE_FULL : IDLE_WFI;
}
