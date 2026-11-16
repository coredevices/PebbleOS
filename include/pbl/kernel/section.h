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
 * @def PBL_ITERABLE_SECTION
 * @brief Add the object to an iterable section, a table gathered at link time from any file.
 *
 * Where the firmware linker script gathers the section, the entries are sorted by @p key, or
 * kept in link order if the section is registered as unsorted; elsewhere they come in no
 * particular order. The section is registered with @c pbl_iterable_section() in CMake, and may
 * be empty. Objects must be constant.
 *
 * @code{.c}
 * // where the table is walked
 * PBL_ITERABLE_SECTION_DECLARE(const struct foo, foos);
 *
 * PBL_ITERABLE_SECTION_FOREACH(const struct foo, foos, foo) {
 *   ...
 * }
 *
 * // any file
 * static const struct foo s_foo PBL_ITERABLE_SECTION(struct foo, foos, "bar") = {...};
 * @endcode
 *
 * @param type Type of the entries.
 * @param tag Table name, a C identifier of at most 14 characters. Its section is named after it
 * with an @c i_ prefix, which fits the 16 characters Mach-O allows.
 * @param key Sort key, a string.
 */

/**
 * @def PBL_ITERABLE_SECTION_DECLARE
 * @brief Declare the bounds of an iterable section.
 *
 * @param type Type of the entries.
 * @param tag Section name.
 */

/**
 * @def PBL_ITERABLE_SECTION_START
 * @brief First entry of an iterable section declared with @ref PBL_ITERABLE_SECTION_DECLARE.
 *
 * @param tag Section name.
 */
#define PBL_ITERABLE_SECTION_START(tag) __start_i_##tag

/**
 * @def PBL_ITERABLE_SECTION_END
 * @brief End of an iterable section declared with @ref PBL_ITERABLE_SECTION_DECLARE.
 *
 * @param tag Section name.
 */
#define PBL_ITERABLE_SECTION_END(tag) __stop_i_##tag

/**
 * @def PBL_ITERABLE_SECTION_FOREACH
 * @brief Loop over the entries of an iterable section.
 *
 * @param type Type of the entries.
 * @param tag Section name.
 * @param var Name of the loop variable, a pointer to the entry.
 */
#define PBL_ITERABLE_SECTION_FOREACH(type, tag, var) \
  for (type *var = PBL_ITERABLE_SECTION_START(tag); var < PBL_ITERABLE_SECTION_END(tag); var++)

#if UNITTEST || defined(PBL_NO_LINKER_SCRIPT)
#ifdef __APPLE__
#define PBL_ITERABLE_SECTION(type, tag, key)                                   \
  PBL_USED PBL_ALIGNED(alignof(type)) PBL_SECTION_IMPL("__DATA_CONST,i_" #tag) \
      PBL_NO_SANITIZE_ADDRESS
#define PBL_ITERABLE_SECTION_DECLARE(type, tag)                                              \
  extern type PBL_ITERABLE_SECTION_START(tag)[] __asm("section$start$__DATA_CONST$i_" #tag); \
  extern type PBL_ITERABLE_SECTION_END(tag)[] __asm("section$end$__DATA_CONST$i_" #tag)
#else
#define PBL_ITERABLE_SECTION(type, tag, key) \
  PBL_USED PBL_ALIGNED(alignof(type)) PBL_SECTION_IMPL("i_" #tag) PBL_NO_SANITIZE_ADDRESS
// ELF linkers only define the bounds of a section that exists
#define PBL_ITERABLE_SECTION_DECLARE(type, tag)           \
  extern type PBL_ITERABLE_SECTION_START(tag)[] PBL_WEAK; \
  extern type PBL_ITERABLE_SECTION_END(tag)[] PBL_WEAK
#endif
#else
#define PBL_ITERABLE_SECTION(type, tag, key) \
  PBL_USED PBL_ALIGNED(alignof(type)) PBL_SECTION_IMPL(".i_" #tag "." key)
#define PBL_ITERABLE_SECTION_DECLARE(type, tag)  \
  extern type PBL_ITERABLE_SECTION_START(tag)[]; \
  extern type PBL_ITERABLE_SECTION_END(tag)[]
#endif

/** @} */
