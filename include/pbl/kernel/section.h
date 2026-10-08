/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

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

/**
 * @def PBL_UNSORTED_SECTION
 * @brief Gather the object in a named section, without a linker script.
 *
 * For builds linked without the firmware linker script (@c PBL_NO_LINKER_SCRIPT), where
 * @ref PBL_SECTION is a no-op: the linker collects such a section on its own, in no particular
 * order, and names its bounds PBL_UNSORTED_SECTION_START() and PBL_UNSORTED_SECTION_END(). The
 * objects must be @ref PBL_USED.
 *
 * @code{.c}
 * extern const struct foo foo_start[] PBL_UNSORTED_SECTION_START(foos);
 * extern const struct foo foo_end[] PBL_UNSORTED_SECTION_END(foos);
 *
 * static const struct foo s_foo PBL_USED PBL_UNSORTED_SECTION(foos) = {...};
 * @endcode
 *
 * @param name Section name, a C identifier of at most 14 characters (Mach-O allows 16).
 */

/**
 * @def PBL_UNSORTED_SECTION_START
 * @brief Name a declaration after the start of a @ref PBL_UNSORTED_SECTION.
 */

/**
 * @def PBL_UNSORTED_SECTION_END
 * @brief Name a declaration after the end of a @ref PBL_UNSORTED_SECTION.
 */

#ifdef __APPLE__
#define PBL_UNSORTED_SECTION(name) PBL_SECTION_IMPL("__DATA_CONST,__" #name) PBL_NO_SANITIZE_ADDRESS
#define PBL_UNSORTED_SECTION_START(name) __asm("section$start$__DATA_CONST$__" #name)
#define PBL_UNSORTED_SECTION_END(name)   __asm("section$end$__DATA_CONST$__" #name)
#else
#define PBL_UNSORTED_SECTION(name)       PBL_SECTION_IMPL(#name) PBL_NO_SANITIZE_ADDRESS
#define PBL_UNSORTED_SECTION_START(name) __asm("__start_" #name)
#define PBL_UNSORTED_SECTION_END(name)   __asm("__stop_" #name)
#endif

/** @} */
