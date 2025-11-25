/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <string.h>

//! \file
//! Calculate the legacy checksum of data.
//!
//! The calculation is somewhat like a CRC with the CRC-32 polynomial, but with
//! the data bytes reordered oddly. The checksum is calculated 32 bits at a
//! time, little-endian, MSB-first. The legacy checksum of bytes A B C D E F G H
//! is equal to the CRC-32 of bytes D C B A H G F E (xor 0xFFFFFFFF). When the
//! data being checksummmed is not a multiple of four bytes in length, the
//! remainder bytes are zero-padded and byte-swapped(!) before being checksummed
//! like the previous full words. For example, the legacy checksum of bytes
//! 1 2 3 4 5 6 is equal to the checksum of bytes 1 2 3 4 6 5 0 0, which is
//! equivalent to the CRC-32 of bytes 4 3 2 1 0 0 5 6 (xor 0xFFFFFFFF).
//!
//! The legacy checksum should not be used except when required for
//! backwards-compatibility purposes.

typedef struct LegacyChecksum {
  uint32_t reg;
  uint8_t accumulator[3];
  uint8_t accumulated_length;
} LegacyChecksum;

void legacy_defective_checksum_init(LegacyChecksum *checksum);

void legacy_defective_checksum_update(
    LegacyChecksum * restrict checksum,
    const void * restrict data, size_t length);

uint32_t legacy_defective_checksum_finish(LegacyChecksum *checksum);

//! Convenience wrapper to checksum memory in one shot.
uint32_t legacy_defective_checksum_memory(const void * restrict data,
                                          size_t length);
