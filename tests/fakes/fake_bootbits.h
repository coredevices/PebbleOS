/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <system/bootbits.h>

void fake_boot_bit_set(BootBitValue bit);
uint32_t fake_boot_bits_get(void);
void fake_boot_bits_reset(void);
