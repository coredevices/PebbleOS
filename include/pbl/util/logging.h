/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup util_logging Library logging
 * @ingroup util
 * @brief Logging hooks of the utility library.
 *
 * The library provides weak implementations printing to stdout; the firmware overrides them to
 * log at INFO level and to write to the debug serial port.
 * @{
 */

/**
 * @brief Log a message.
 *
 * @param filename Source file name.
 * @param line Source line number.
 * @param string Message.
 */
void util_log(const char *filename, int line, const char *string);

/**
 * @brief Log a message with the current file name and line.
 *
 * @param string Message.
 */
#define UTIL_LOG(string)                       \
  do {                                         \
    util_log(__FILE_NAME__, __LINE__, string); \
  } while (0)

/**
 * @brief Write a line straight to the debug serial port.
 *
 * @param string Line, without line terminator.
 */
void util_dbgserial_str(const char *string);

/** @} */
