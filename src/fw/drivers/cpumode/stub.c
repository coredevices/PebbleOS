/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/cpumode.h"

static uint32_t s_current_mhz = CPUMODE_FREQ_HIGH_MHZ;

void cpumode_set_freq_mhz(uint32_t freq_mhz) {
  if (freq_mhz == CPUMODE_FREQ_LIGHT_MHZ || freq_mhz == CPUMODE_FREQ_MEDIUM_MHZ ||
      freq_mhz == CPUMODE_FREQ_HIGH_MHZ) {
    s_current_mhz = freq_mhz;
  }
}

uint32_t cpumode_get_freq_mhz(void) {
  return s_current_mhz;
}

void cpumode_set(CPUMode mode) {
  cpumode_set_freq_mhz(mode == CPUMode_LowPower ? CPUMODE_FREQ_LIGHT_MHZ : CPUMODE_FREQ_HIGH_MHZ);
}
