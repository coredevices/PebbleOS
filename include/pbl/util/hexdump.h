/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_hexdump Hex dump
 * @ingroup util
 * @brief xxd-style hex dumps through a line callback.
 * @{
 */

/**
 * @brief Output callback of hexdump(), called once per line.
 *
 * @param level Level passed to hexdump().
 * @param src_filename Source file name passed to hexdump().
 * @param src_line_number Source line number passed to hexdump().
 * @param line_buffer Line, without line terminator.
 */
typedef void (*HexdumpLineCallback)(int level, const char *src_filename, int src_line_number,
                                    const char *line_buffer);

/**
 * @brief Hex dump data in xxd-style lines of 16 bytes.
 *
 * Each line has the offset, the bytes in hex and their printable ASCII characters.
 *
 * @param src_filename Source file name, passed through to @p write_line_cb.
 * @param src_line_number Source line number, passed through to @p write_line_cb.
 * @param level Log level, passed through to @p write_line_cb.
 * @param data Data to dump.
 * @param length Length of @p data in bytes.
 * @param write_line_cb Output callback.
 */
void hexdump(const char *src_filename, int src_line_number, int level, const uint8_t *data,
             size_t length, HexdumpLineCallback write_line_cb);

/** @} */
