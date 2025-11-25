/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/attributes.h"

static inline bool mcu_state_is_thread_privileged(void) {
  return true;
}

void WEAK mcu_state_set_thread_privilege(bool privilege) {
}

bool WEAK mcu_state_is_privileged(void) {
  return true;
}
