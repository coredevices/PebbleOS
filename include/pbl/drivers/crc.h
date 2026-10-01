/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

//! Hardware CRC acceleration used by subsys/crc, provided by drivers that select CRC_HW_ACCEL.
//!
//! Each function processes a prefix of the data, a whole number of 32-bit words, and returns its
//! length in bytes. It returns 0 without touching the running value when the hardware cannot
//! take the request; subsys/crc computes whatever is left in software.

//! @param[in,out] crc Running CRC-32/ISO-HDLC value, as passed to pbl_crc32()
//! @param data Data to process
//! @param len Length of data in bytes
//! @return Number of bytes processed, a multiple of 4
size_t pbl_crc_hw_crc32(uint32_t *crc, const void *data, size_t len);

//! @param[in,out] reg Running legacy checksum register, see struct pbl_crc32_legacy
//! @param data Data to process, as little-endian 32-bit words
//! @param len Length of data in bytes
//! @return Number of bytes processed, a multiple of 4
size_t pbl_crc_hw_crc32_legacy(uint32_t *reg, const void *data, size_t len);
