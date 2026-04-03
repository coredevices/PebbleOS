/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

typedef enum {
  CPUMode_LowPower = 0,
  CPUMode_HighPerformance = 1,
} CPUMode;

void cpumode_set(CPUMode mode);

