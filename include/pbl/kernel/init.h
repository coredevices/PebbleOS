/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup kernel_init Boot
 * @ingroup kernel
 * @brief SoC hook in the kernel's reset path.
 *
 * The kernel owns the reset vector: it copies @c .data (and @c .ramfunc with
 * @c CONFIG_RAMFUNC), zeroes @c .bss and calls pbl_soc_early_init(). It then sets up interrupts,
 * faults, the heap and the early console, prints the boot banner and calls main().
 * @{
 */

/**
 * @brief Early SoC initialization: vendor system init, clocks and caches.
 *
 * Implemented by the SoC. Runs on the ISR stack once @c .data, @c .bss and @c .ramfunc are set
 * up, before main().
 */
void pbl_soc_early_init(void);

/** @} */
