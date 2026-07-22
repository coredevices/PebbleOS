/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef struct AW86225Config {
  uint16_t lra_frequency_hz;
  uint16_t lra_frequency_tolerance_hz;
} AW86225Config;
