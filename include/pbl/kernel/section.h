/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/kernel/compiler.h"

#ifdef CONFIG_RAMFUNC
//! Run the function from RAM.
#define PBL_SECTION_RAM PBL_NOINLINE PBL_SECTION(".ramfunc.text")
//! Place the constant in RAM, for tables used by PBL_SECTION_RAM functions.
#define PBL_SECTION_RAM_RODATA PBL_SECTION(".ramfunc.rodata")
#else
#define PBL_SECTION_RAM
#define PBL_SECTION_RAM_RODATA
#endif

#ifdef CONFIG_EXTRAM
//! Place the zero-initialized variable in external RAM.
#define PBL_SECTION_EXTRAM PBL_SECTION(".extram.bss")
#else
#define PBL_SECTION_EXTRAM
#endif
