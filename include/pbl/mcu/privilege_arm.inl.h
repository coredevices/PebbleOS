/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

/** @cond INTERNAL_HIDDEN */

#include <cmsis_core.h>

// CONTROL bit 0 is nPRIV: 0 = privileged, 1 = unprivileged thread mode. Readable in both
// modes, writable only when privileged.

static inline bool mcu_state_is_thread_privileged(void) {
  return (__get_CONTROL() & 0x1) == 0;
}

/** @endcond */
