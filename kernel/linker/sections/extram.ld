/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .extram_bss (NOLOAD) : ALIGN(32) {
        __extram_bss_start = .;

        *(.extram.bss)
        *(.extram.bss.*)

        . = ALIGN(32);
        __extram_bss_end = .;
    } >EXTRAM
