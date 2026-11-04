/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>

//! Binary data on the text console travels in frames between text lines:
//!   ESC _ <channel> <base64(seq, payload, crc32)> ESC \ (backslash)
//!
//! The crc32 covers the channel, seq and payload. The host sends one frame at a time and repeats
//! it until acknowledged, with a seq that changes per frame, or 0 where repeats do no harm.
//! The firmware does not wait for the host, and sends seq 0.

#define CONSOLE_FRAME_MTU 512

#define CONSOLE_FRAME_CHANNEL_ACK 'A'
#define CONSOLE_FRAME_CHANNEL_PP  'P'

//! Feed a received character, passing those outside frames to @p text_input.
void console_frame_input_from_isr(char c, void (*text_input)(char c));

//! Send a frame of at most CONSOLE_FRAME_MTU bytes. Not for interrupt context.
void console_frame_send(char channel, const void *data, size_t len);

//! Handle a frame on the Pebble protocol channel, on KernelBG.
void console_pp_handle_frame(const void *data, size_t len);
