/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

uint32_t sle_decode(const uint8_t *in, uint32_t in_len, uint8_t *out, uint32_t out_len);
