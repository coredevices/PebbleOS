/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pbl/console/console.h>

#define CONSOLE_FRAME_CHANNEL_ACK     'A'
#define CONSOLE_FRAME_CHANNEL_IMAGING 'B'

//! Feed a received character, passing those outside frames to @p text_input.
void pbl_console__frame_input_from_isr(char c, void (*text_input)(char c));

//! Handle a frame on the flash imaging channel, on SysWorkq.
void pbl_console__imaging_handle_frame(const void *data, size_t len);
