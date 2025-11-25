/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

inline static bool mcu_state_is_thread_privileged(void);

//! Update the thread mode privilege bit in the control register. Note that you
//! must already be privileged to call this with a true argument.
void mcu_state_set_thread_privilege(bool privilege);

bool mcu_state_is_privileged(void);

#ifdef __arm__
#include "mcu/privilege_arm.inl.h"
#else
#include "mcu/privilege_stubs.inl.h"
#endif
