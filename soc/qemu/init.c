/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

// System clock frequency for QEMU (64 MHz)
uint32_t SystemCoreClock = 64000000;

// SoC early init - nothing to do for QEMU
void soc_early_init(void) {
}
