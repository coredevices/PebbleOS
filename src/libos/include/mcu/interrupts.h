/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

inline static bool mcu_state_is_isr(void);

//! Return the priority level of the currently executing exception handler.
//! Returns ~0 if not in an exception handler. Lower numbers mean higher
//! priority. Anything below 0xB should not execute any FreeRTOS calls.
inline static uint32_t mcu_state_get_isr_priority(void);

bool mcu_state_are_interrupts_enabled(void);

#ifdef __arm__
#include "mcu/interrupts_arm.inl.h"
#else
#include "mcu/interrupts_stubs.inl.h"
#endif
