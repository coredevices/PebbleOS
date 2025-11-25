/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

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
