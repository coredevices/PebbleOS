/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /* the program code is stored in the .text section, which goes to Flash */
    .text : {
        __syscall_text_start__ = .;
        *(.syscall_text.*)           /* System call functions */
        __syscall_text_end__ = .;
        *(.text)                   /* remaining code */
        *(.text.*)                 /* remaining code */
        *(.rodata)                 /* read-only data (constants) */
        *(.rodata*)
        *(.constdata)              /* read-only data (constants) */
        *(.constdata*)
        *(.conststring)
        *(i.*)

        . = ALIGN(8);
    } >FLASH
