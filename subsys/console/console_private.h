/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! Binary data on the text console travels in frames between text lines:
//!   ESC _ <channel> <base64(seq, payload, crc32)> ESC \ (backslash)
//!
//! The crc32 covers the channel, seq and payload. A sender repeats a frame until acknowledged, one
//! at a time, with a seq that changes per frame. Seq 0 is for frames nobody acknowledges or waits
//! for: USB serial adapters lose data, so the Pebble protocol stream from the firmware needs a seq.

#define CONSOLE_FRAME_MTU 512

#define CONSOLE_FRAME_CHANNEL_ACK     'A'
#define CONSOLE_FRAME_CHANNEL_PP      'P'
#define CONSOLE_FRAME_CHANNEL_IMAGING 'B'

//! Feed a received character, passing those outside frames to @p text_input.
void pbl_console__frame_input_from_isr(char c, void (*text_input)(char c));

//! Send a frame of at most CONSOLE_FRAME_MTU bytes. Not for interrupt context.
void pbl_console__frame_send(char channel, uint8_t seq, const void *data, size_t len);

//! Handle a frame on the Pebble protocol channel, on KernelBG.
void pbl_console__pp_handle_frame(const void *data, size_t len);

//! Handle the host's acknowledgment of a Pebble protocol frame, on the timer task: those sending
//! on KernelBG or KernelMain may be blocked waiting for what this frees.
void pbl_console__pp_handle_ack(uint8_t seq);

//! Handle a frame on the flash imaging channel, on KernelBG.
void pbl_console__imaging_handle_frame(const void *data, size_t len);
