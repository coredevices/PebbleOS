/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash.h>
#include <pbl/kernel/idle.h>
#include <pbl/soc/nrf/idle.h>
#include <pbl/util/size.h>

#ifdef CONFIG_FLASH
static void prv_wfi_cb(void *arg) {
  pbl_cpu_idle();
}

// WFI with the external flash powered down.
static void prv_full_sleep(void) {
  pbl_flash_power_down(FLASH, prv_wfi_cb, nullptr);
}
#endif // CONFIG_FLASH

const struct pbl_idle_state pbl_soc_idle_states[] = {
  [SOC_NRF_IDLE_WFI] = {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = pbl_cpu_idle},
#ifdef CONFIG_FLASH
  [SOC_NRF_IDLE_FULL] = {.name = "full", .min_ticks = 5, .exit_ticks = 2, .enter = prv_full_sleep},
#endif
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

int pbl_soc_idle_state_limit(void) {
  return SOC_NRF_IDLE_FULL;
}
