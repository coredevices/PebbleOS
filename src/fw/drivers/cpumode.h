/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

typedef enum {
  CPUMode_LowPower = 0,
  CPUMode_HighPerformance = 1,
} CPUMode;

#define CPUMODE_FREQ_LIGHT_MHZ 48
#define CPUMODE_FREQ_MEDIUM_MHZ 144
#define CPUMODE_FREQ_HIGH_MHZ 240

//! Set CPU frequency in MHz. Supported values: 48, 144, 240.
void cpumode_set_freq_mhz(uint32_t freq_mhz);

//! Return the last frequency set via cpumode_set_freq_mhz() or cpumode_set().
uint32_t cpumode_get_freq_mhz(void);

void cpumode_set(CPUMode mode);
