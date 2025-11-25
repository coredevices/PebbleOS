/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void pulse_logging_init(void);

//! Log a message using PULSEv2
void pulse_logging_log(uint8_t log_level, const char* src_filename,
                       uint16_t src_line_number, const char* message);

//! Log a message using PULSEv2 synchronously, even from a critical section
void pulse_logging_log_sync(
    uint8_t log_level, const char* src_filename,
    uint16_t src_line_number, const char* message);

//! Log a message from a fault handler by concatenating several strings.
void *pulse_logging_log_sync_begin(
    uint8_t log_level, const char *src_filename, uint16_t src_line_number);
void pulse_logging_log_sync_append(void *ctx, const char *message);
void pulse_logging_log_sync_send(void *ctx);

//! Flush the ISR log buffer. Call this when crashing.
void pulse_logging_log_buffer_flush(void);
