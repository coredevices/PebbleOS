/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#if defined(__clang__)
#define GCC_ONLY(x)
#else
#define GCC_ONLY(x) x
#endif

// Function attributes
#define FORMAT_FUNC(TYPE, STR_IDX, FIRST) __attribute__((__format__(TYPE, STR_IDX, FIRST)))

#define FORMAT_PRINTF(STR_IDX, FIRST) FORMAT_FUNC(__printf__, STR_IDX, FIRST)

#define ALWAYS_INLINE __attribute__((__always_inline__)) inline
#define DEPRECATED __attribute__((deprecated))
#define NOINLINE __attribute__((__noinline__))
#define NORETURN __attribute__((__noreturn__)) void
#define NAKED_FUNC __attribute__((__naked__))
#define OPTIMIZE_FUNC(LVL) GCC_ONLY(__attribute__((__optimize__(LVL))))
#define CONST_FUNC __attribute__((__const__))
#define PURE_FUNC __attribute__((__pure__))

// Variable attributes
#define ATTR_CLEANUP(FUNC) __attribute__((__cleanup__(FUNC)))

// Structure attributes
#define PACKED __attribute__((__packed__))

// General attributes
#define USED __attribute__((__used__))
#define PBL_UNUSED __attribute__((__unused__))
#define WEAK __attribute__((__weak__))
#define ALIAS(sym) __attribute__((__weak__, __alias__(sym)))
#define EXTERNALLY_VISIBLE GCC_ONLY(__attribute__((__externally_visible__)))
#define ALIGN(bytes) __attribute__((__aligned__(bytes)))

// Unit tests break if variables go in custom sections
#if !UNITTEST
# define SECTION(SEC) __attribute__((__section__(SEC)))
#else
# define SECTION(SEC)
#endif

// Only present on STM32F7
#define DTCM_BSS   SECTION(".dtcm_bss")

// DMA_BSS: Section attribute for DMA buffers
#if MICRO_FAMILY_STM32F7
# define DMA_BSS    DTCM_BSS
// There is an erratum present in STM32F7xx which causes DMA reads from DTCM
// (but not writes to DTCM) to be corrupted if the MCU enters sleep mode during
// the transfer. Source DMA buffers must be placed in SRAM on these platforms.
// The DMA driver enforces this. Also, alignment to the start of a cache line
// seems to be required, though it's unclear why.
# define DMA_READ_BSS ALIGN(32)
#else
# define DMA_BSS
# define DMA_READ_BSS
#endif

// Use this macro to allow overriding of private functions in order to test them within unit tests.
#if !UNITTEST
# define T_STATIC static
#else
# define T_STATIC WEAK
#endif

// Use this macro to allow overriding of non-static (i.e. global) functions in order to test them
// within unit tests. For lack of a better name, we have named this a MOCKABLE (i.e. can be
// mocked or overridden in unit tests but not in normal firmware)
#if !UNITTEST
# define MOCKABLE
#else
# define MOCKABLE WEAK
#endif
