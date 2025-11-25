/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "system/logging.h"
#include "util/attributes.h"

#include <stdint.h>
#include <stdbool.h>

typedef struct PACKED LogBinaryMessage {
  uint32_t timestamp;
  uint8_t log_level;
  uint8_t message_length;
  uint16_t line_number;
  char filename[16];
  char message[];
} LogBinaryMessage;


//! This structure encapsulates the buffers and state used for formatting a log message.
typedef struct {
  bool in_progress;                                 // Set true while a log is in progress
  char buffer[LOG_BUFFER_LENGTH];                   // For construction of the final log message
} LogState;

//! Return a single character representing the current log level. Used in serial logging.
char pbl_log_get_level_char(const uint8_t log_level);

//! Log a message to whatever specific channels are appropriate based on context and configuration.
//! Internally calls kernel_pbl_log_serial and kernel_pbl_log_flash.
void kernel_pbl_log(LogBinaryMessage* log_message, bool async);

//! Force a log message out the serial channel.
void kernel_pbl_log_serial(LogBinaryMessage *log_message, bool async);

//! Force a log message out the serial channel from a fault handler or
//! other context where OS services are unavailable or can't be trusted,
//! and where stack space is at a premium.
void kernel_pbl_log_from_fault_handler(
    const char *src_filename, uint16_t src_line_number, const char *message);

void kernel_pbl_log_from_fault_handler_fmt(
    const char *src_filename, uint16_t src_line_number, char *buffer,
    unsigned int buffer_len, const char *fmt, ...);

#define PBL_LOG_FROM_FAULT_HANDLER(message) \
  kernel_pbl_log_from_fault_handler(__FILE_NAME__, __LINE__, message)

#define PBL_LOG_FROM_FAULT_HANDLER_FMT(buffer, buffer_len, fmt, ...) \
  kernel_pbl_log_from_fault_handler_fmt( \
      __FILE_NAME__, __LINE__, buffer, buffer_len, fmt, __VA_ARGS__)
