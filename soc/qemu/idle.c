/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/idle.h>
#include <pbl/util/size.h>

#include <cmsis_core.h>

static void prv_wfi(void) {
  __DSB();
  __WFI();
  __ISB();
}

const struct pbl_idle_state pbl_soc_idle_states[] = {
  {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = prv_wfi},
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

int pbl_soc_idle_state_limit(void) {
  return 0;
}
