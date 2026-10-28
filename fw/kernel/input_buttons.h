/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/button_id.h>

//! Input key code that acts as a button.
uint16_t input_buttons_code(ButtonId id);

//! Starts turning key events into button events. Keys are tracked from boot, but a press made
//! during boot, such as the one that turned the watch on, must not reach the event queue.
void input_buttons_enable(void);

//! Swaps up and down while the watch is rotated by 180 degrees.
void input_buttons_set_rotated(bool rotated);

//! Bitmask of the pressed buttons, bit n for ButtonId n.
uint8_t input_buttons_get_state(void);
