/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup crc CRC
 * @ingroup subsys
 * @brief Checksums stored or exchanged by the firmware.
 *
 * The software implementations are always built. Buffers of at least
 * @c CONFIG_CRC_HW_MIN_LEN bytes are offloaded to a hardware CRC unit when a
 * driver provides one (see @ref drivers_crc).
 *
 * Every function takes the running value and returns the updated one, so data
 * can be fed in chunks:
 *
 * @code{.c}
 * uint32_t crc = pbl_crc32(0, &hdr, sizeof(hdr));
 * crc = pbl_crc32(crc, payload, payload_len);
 * @endcode
 * @{
 */

/**
 * @brief CRC-8/OPENSAFETY.
 *
 * Polynomial 0x2F, zero initial value, no reflection and no final XOR.
 *
 * @param crc Running value, 0 to start.
 * @param data Data to checksum.
 * @param len Length of @p data in bytes.
 * @return Updated CRC.
 */
uint8_t pbl_crc8(uint8_t crc, const void *data, size_t len);

/**
 * @brief pbl_crc8() consuming data from its last byte to its first.
 *
 * @param crc Running value, 0 to start.
 * @param data Data to checksum.
 * @param len Length of @p data in bytes.
 * @return Updated CRC.
 */
uint8_t pbl_crc8_reversed(uint8_t crc, const void *data, size_t len);

/**
 * @brief CRC-32/ISO-HDLC, as used by zlib and PNG.
 *
 * Compatible with zlib's crc32().
 *
 * @param crc Running value, 0 to start.
 * @param data Data to checksum.
 * @param len Length of @p data in bytes.
 * @return Updated CRC.
 */
uint32_t pbl_crc32(uint32_t crc, const void *data, size_t len);

/**
 * @brief CRC-32 of a message with its little-endian CRC-32 appended, when the
 * message is intact.
 */
#define PBL_CRC32_RESIDUE 0x2144DF1CU

/**
 * @brief Running state of the legacy checksum.
 *
 * The legacy checksum is the CRC-32/MPEG-2 computed by the STM32F2/F4 CRC unit over
 * little-endian 32-bit words, MSB first: the checksum of bytes A B C D E F G H is the CRC of
 * D C B A H G F E. A trailing partial word is zero-padded at the front, so bytes 1 2 3 4 5 6 are
 * checksummed as 4 3 2 1 0 0 5 6. It persists in on-flash and on-wire formats.
 *
 * @code{.c}
 * struct pbl_crc32_legacy ctx;
 *
 * pbl_crc32_legacy_init(&ctx);
 * pbl_crc32_legacy_update(&ctx, hdr, hdr_len);
 * pbl_crc32_legacy_update(&ctx, body, body_len);
 * uint32_t crc = pbl_crc32_legacy_finish(&ctx);
 * @endcode
 */
struct pbl_crc32_legacy {
  /** CRC register. */
  uint32_t reg;
  /** Bytes of an incomplete word, pending until the next update or finish. */
  uint8_t accumulator[3];
  /** Number of valid bytes in @ref accumulator. */
  uint8_t accumulated_length;
};

/**
 * @brief Start a legacy checksum.
 *
 * @param ctx Checksum state.
 */
void pbl_crc32_legacy_init(struct pbl_crc32_legacy *ctx);

/**
 * @brief Feed data to a legacy checksum.
 *
 * @param ctx Checksum state.
 * @param data Data to checksum.
 * @param len Length of @p data in bytes.
 */
void pbl_crc32_legacy_update(struct pbl_crc32_legacy *ctx, const void *data, size_t len);

/**
 * @brief Finish a legacy checksum.
 *
 * Pads and consumes any trailing partial word.
 *
 * @param ctx Checksum state.
 * @return Checksum of all data fed since pbl_crc32_legacy_init().
 */
uint32_t pbl_crc32_legacy_finish(struct pbl_crc32_legacy *ctx);

/**
 * @brief Legacy checksum of a single buffer.
 *
 * @param data Data to checksum.
 * @param len Length of @p data in bytes.
 * @return Checksum.
 */
uint32_t pbl_crc32_legacy(const void *data, size_t len);

/** @} */
