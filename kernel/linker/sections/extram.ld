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

    .disposable_bss (NOLOAD) : ALIGN(32) {
        __disposable_bss_start = .;

        *(.disposable.bss)
        *(.disposable.bss.*)

        . = ALIGN(32);
        __disposable_bss_end = .;
    } >EXTRAM
