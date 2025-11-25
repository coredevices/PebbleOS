/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

void crc_init(void);

/*
 * calculate the CRC32 for a stream of bytes.
 * NOTE: not safe to call from ISR
 */
uint32_t crc_calculate_bytes(const uint8_t* data, unsigned int data_length);

/*
 * calculate the CRC32 for a stream of bytes from flash
 * NOTE: not safe to call from ISR
 */
uint32_t crc_calculate_flash(uint32_t address, unsigned int num_bytes);

/*
 * calculate a 8-bit CRC of a given byte sequence. Note that this is not using
 * the standard CRC-8 polynomial, because the standard polynomial isn't very
 * good.
 */
uint8_t crc8_calculate_bytes(const uint8_t *data, unsigned int data_length);

void crc_calculate_incremental_start(void);

void crc_calculate_incremental_stop(void);
