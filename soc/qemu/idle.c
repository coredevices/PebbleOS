/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/idle.h>
#include <pbl/util/size.h>

const struct pbl_idle_state pbl_soc_idle_states[] = {
  {.name = "wfi", .min_ticks = 1, .exit_ticks = 0, .enter = pbl_cpu_idle},
};
const size_t pbl_soc_num_idle_states = ARRAY_LENGTH(pbl_soc_idle_states);

int pbl_soc_idle_state_limit(void) {
  return 0;
}
