/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#define PULSE_MAX_RECEIVE_UNIT (1500)
#define PULSE_MIN_FRAME_LENGTH (6)

//! Start a PULSE session on dbgserial.
void pulse_start(void);

//! End a PULSE session on dbgserial.
void pulse_end(void);

//! PULSE ISR receive character handler.
void pulse_handle_character(char c);
