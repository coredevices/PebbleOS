/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! CRC-8 with polynomial 0x2F, zero initial value, no reflection and no final XOR
//! (CRC-8/OPENSAFETY). Start with 0 and feed the result back to continue.
uint8_t pbl_crc8(uint8_t crc, const void *data, size_t len);

//! pbl_crc8() consuming data from its last byte to its first.
uint8_t pbl_crc8_reversed(uint8_t crc, const void *data, size_t len);

//! CRC-32/ISO-HDLC, as used by zlib and PNG. Start with 0 and feed the result back to continue,
//! like zlib's crc32().
uint32_t pbl_crc32(uint32_t crc, const void *data, size_t len);

//! CRC-32 of a message with its little-endian CRC-32 appended, when the message is intact.
#define PBL_CRC32_RESIDUE 0x2144DF1CU

//! Running state of the legacy checksum.
//!
//! The legacy checksum is the CRC-32/MPEG-2 computed by the STM32F2/F4 CRC unit over
//! little-endian 32-bit words, MSB first: the checksum of bytes A B C D E F G H is the CRC of
//! D C B A H G F E. A trailing partial word is zero-padded at the front, so bytes 1 2 3 4 5 6 are
//! checksummed as 4 3 2 1 0 0 5 6. It persists in on-flash and on-wire formats.
struct pbl_crc32_legacy {
  uint32_t reg;
  uint8_t accumulator[3];
  uint8_t accumulated_length;
};

void pbl_crc32_legacy_init(struct pbl_crc32_legacy *ctx);

void pbl_crc32_legacy_update(struct pbl_crc32_legacy *ctx, const void *data, size_t len);

uint32_t pbl_crc32_legacy_finish(struct pbl_crc32_legacy *ctx);

//! Legacy checksum of a single buffer.
uint32_t pbl_crc32_legacy(const void *data, size_t len);
