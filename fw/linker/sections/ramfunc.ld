/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .ramfunc : {
        . = ALIGN(4);
        __ramfunc_start = .;
        *(.ramfunc.text .ramfunc.text.*)
        *(.ramfunc.rodata .ramfunc.rodata.*)
#include <ramfunc-objects.ld>
#include <snippets-ramfunc.ld>
        . = ALIGN(4);
        __ramfunc_end = .;
    } >RAM AT>FLASH
    __ramfunc_load_start = LOADADDR(.ramfunc);
