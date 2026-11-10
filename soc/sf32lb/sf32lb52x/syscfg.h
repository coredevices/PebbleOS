/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

// Loads the factory calibration from the efuse and applies the PMU trims
void soc_sf32lb52_syscfg_init(void);

// Factory trims of the HPSYS LDO for the S0 and S1 voltage modes. Returns 0, or -ENODATA on an
// uncalibrated part.
int soc_sf32lb52_hpsys_ldo_trims(uint8_t *s0, uint8_t *s1);
