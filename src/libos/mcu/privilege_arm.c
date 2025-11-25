/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "mcu/privilege.h"

#include "mcu/interrupts.h"
#include "util/attributes.h"

// These functions need to be called from assembly so they can't be inlined
EXTERNALLY_VISIBLE void mcu_state_set_thread_privilege(bool privileged) {
  uint32_t control = __get_CONTROL();
  if (privileged) {
    control &= ~0x1;
  } else {
    control |= 0x1;
  }
  __set_CONTROL(control);
}

bool mcu_state_is_privileged(void) {
  return mcu_state_is_thread_privileged() || mcu_state_is_isr();
}
