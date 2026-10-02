/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

/**
 * @defgroup kernel_section Code and data placement
 * @ingroup kernel
 * @brief Placement of code and data in special sections.
 *
 * SoCs that need code to run while flash is unavailable select @c CONFIG_RAMFUNC, which adds a
 * @c .ramfunc section loaded from flash and copied to RAM at boot. Without it these markers are
 * no-ops and everything stays in flash.
 *
 * @code{.c}
 * static const uint8_t s_table[] PBL_SECTION_RAM_RODATA = {1, 2, 4, 8};
 *
 * PBL_SECTION_RAM void prv_enter_sleep(void) {
 *   // flash is off here
 * }
 * @endcode
 * @{
 */

/**
 * @def PBL_SECTION_RAM
 * @brief Run the function from RAM; also prevents inlining it into flash code.
 */

/**
 * @def PBL_SECTION_RAM_RODATA
 * @brief Place the constant in RAM, for tables used by @ref PBL_SECTION_RAM functions.
 */

#ifdef CONFIG_RAMFUNC
#define PBL_SECTION_RAM        PBL_NOINLINE PBL_SECTION(".ramfunc.text")
#define PBL_SECTION_RAM_RODATA PBL_SECTION(".ramfunc.rodata")
#else
#define PBL_SECTION_RAM
#define PBL_SECTION_RAM_RODATA
#endif

/** @} */
