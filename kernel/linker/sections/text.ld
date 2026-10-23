/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /* the program code is stored in the .text section, which goes to Flash */
    .text : {
        __syscall_text_start__ = .;
        *(.syscall_text.*)           /* System call functions */
        __syscall_checked_text_start__ = .;
        *(.syscall_checked.*)        /* System calls defined with PBL_SYSCALL */
        __syscall_text_end__ = .;
        *(.text)                   /* remaining code */
        *(.text.*)                 /* remaining code */
        . = ALIGN(4);
        __syscall_desc_start__ = .;
        KEEP(*(.syscall_desc))       /* PBL_SYSCALL descriptors (CONFIG_SYSCALL_DESC) */
        __syscall_desc_end__ = .;
        *(.rodata)                 /* read-only data (constants) */
        *(.rodata*)
        *(.constdata)              /* read-only data (constants) */
        *(.constdata*)
        *(.conststring)
        *(i.*)

        . = ALIGN(8);
    } >FLASH
