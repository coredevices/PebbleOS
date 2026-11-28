/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /* The core starts at the start of the image */
    .reset : {
        ASSERT(. == ORIGIN(FLASH), "Error: Reset entry is offset from the start of the FLASH region");
        KEEP(*(.text.arch_riscv_reset))
    } >FLASH
