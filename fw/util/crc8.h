/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Calculate an 8-bit CRC of a given byte sequence. Note that this is not using the standard CRC-8
 * polynomial, because the standard polynomial isn't very good. If the big_endian flag is set, the
 * crc will be calculated by going through the data in reverse order (high->low index).
 */
uint8_t crc8_calculate_bytes(const uint8_t *data, uint32_t data_length, bool big_endian);
void crc8_calculate_bytes_streaming(const uint8_t *data, uint32_t data_length, uint8_t *crc,
                                    bool big_endian);
