/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/** @cond INTERNAL_HIDDEN */

#include <cmsis_core.h>

static inline bool mcu_state_is_isr(void) {
  return __get_IPSR() != 0;
}

/** @endcond */
