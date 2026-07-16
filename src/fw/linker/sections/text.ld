/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /* Exception handling sections. "contains index entries for section unwinding"
     * We don't actually use this or know what it is, but it seems happy to go here. */
    .ARM.exidx :
    {
        . = ALIGN(4);
        __exidx_start = .;
        *(.ARM.exidx)
        *(.ARM.exidx*)
        __exidx_end = .;
        . = ALIGN(4);
    } >FLASH

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
