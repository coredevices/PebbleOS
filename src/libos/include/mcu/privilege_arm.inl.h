/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define CMSIS_COMPATIBLE
#include <mcu.h>


//! @file privilege_arm.inl.h
//! Helpful functions for dealing with our micros execution state.
//!
//! Functions in this file muck with the control register. This register is described here:
//! http://infocenter.arm.com/help/topic/com.arm.doc.dui0552a/DUI0552A_cortex_m3_dgug.pdf page 2-9
//! We only really care about the 0th bit.
//! [0] nPriv Defines the Thread mode privilege level
//!           0 = Privileged
//!           1 = Unprivileged
//! This variable can be read in both modes, but only may be written in privileged mode.

// See http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/CHDIGFCA.html
// for a more detailed explanation of these various privilege states.

static inline bool mcu_state_is_thread_privileged(void) {
  return (__get_CONTROL() & 0x1) == 0;
}
