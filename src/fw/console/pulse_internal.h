/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PULSE_KEEPALIVE_TIMEOUT_DECISECONDS (30)
#if PULSE_EVERYWHERE
#define PULSE_MAX_RECEIVE_UNIT (1500)
#define PULSE_MIN_FRAME_LENGTH (6)
#else
#define PULSE_MAX_RECEIVE_UNIT (520)
#define PULSE_MIN_FRAME_LENGTH (5)
#endif

//! Start a PULSE session on dbgserial.
void pulse_start(void);

//! End a PULSE session on dbgserial.
void pulse_end(void);

//! Terminate the PULSE session in preparation for the firmware to crash.
void pulse_prepare_to_crash(void);

//! PULSE ISR receive character handler.
void pulse_handle_character(char c, bool *should_context_switch);

//! Change the dbgserial baud rate.
void pulse_change_baud_rate(uint32_t new_baud);
