/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <inttypes.h>
#include <stdbool.h>

//!
//! To work as expected:
//!  - Flash logging needs to span at least two erase units
//!  - Flash logging needs to be aligned on erase unit boundaries
//!

void flash_logging_init(void);

#define FLASH_LOG_INVALID_ADDR UINT32_MAX

//! Find space for a log message with a given size and write the header.
//!
//! @return The flash address the message should start at or
//! FLASH_LOG_INVALID_ADDR if no address could be allocated
uint32_t flash_logging_log_start(uint8_t msg_length);

//! Performs a log message write
//!
//! @return True if the message write was successful, false otherwise
bool flash_logging_write(const uint8_t *data_to_write, uint32_t flash_addr,
    uint32_t data_length);

//! Allows a user to disable/enable flash logging after flash_logging_init()
//! has been called.
void flash_logging_set_enabled(bool enabled);

typedef bool (*DumpLineCallback)(uint8_t *message, uint32_t total_length);
typedef void (*DumpCompletedCallback)(bool success);

//! Dump the flash logs of a given generation number
//!
//! @param generation - The saved logs to dump. Generation number indicates
//!    what boot we want to grab logs from where 0 indicates the current boot, 1
//!    indicates the previous boot, etc
//!
//! @param line_cb - The callback to invoke on each log message found for the
//!    specified generation
//!
//! @param completed_cb - The callback to invoke after all messages have been sent to
//!     the line_cb. This is also called (with false) if the generation does not exist.
//!
//! @return True if the log generation existed
bool flash_dump_log_file(int generation, DumpLineCallback line_cb,
                         DumpCompletedCallback completed_cb);
