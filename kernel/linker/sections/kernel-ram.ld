/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .kernel_data : ALIGN(32) {
        __data_start = .; /* This is used by the startup in order to initialize the .data section */

        /* The Chat app's UI was an SDK app; it resets these on every launch (chat_app.c). */
        __chat_ui_data_start = .;
        *chat_ui.c.obj(.data .data.*)
        __chat_ui_data_end = .;

        *(.data)
        *(.data.*)

        . = ALIGN(8);
        __data_end = .;
    } >RAM AT>FLASH
    __data_load_start = LOADADDR(.kernel_data);
    __chat_ui_data_load = __data_load_start + (__chat_ui_data_start - __data_start);

    .kernel_ro_bss (NOLOAD) : {
        __bss_start = .; /* This is used by the startup in order to initialize the .bss section */

        . = ALIGN(__unpriv_ro_bss_size__);
        __unpriv_ro_bss_start__ = .;

        *:lib_a-timelocal.o(.bss.*)
        *:lib_a-tzvars.o(.bss.*)

        *(.kernel_unpriv_ro_bss)

        . = __unpriv_ro_bss_start__ + __unpriv_ro_bss_size__;
    } >RAM

    .kernel_bss (NOLOAD) : {
        *(.freertos_privileged_data)

        __chat_ui_bss_start = .;
        *chat_ui.c.obj(.bss .bss.* COMMON)
        __chat_ui_bss_end = .;

        *(.bss)
        *(.bss.*)

        *(COMMON)

        __bss_end = .;
    } >RAM

    .stack (NOLOAD) : {
        . = ALIGN(CONFIG_KERNEL_STACK_ALIGN);
        __isr_stack_start__ = .;

        . = . + CONFIG_KERNEL_STACK_GUARD_SIZE + CONFIG_KERNEL_ISR_STACK_SIZE;

        _estack = .;
    } >REGION_ISR_STACK

    /* The heap gets whatever is left. Keep the section empty so the memory
       usage report shows static usage only. */
    .kernel_heap (NOLOAD) : {
        _heap_start = .;
    } >REGION_KERNEL_HEAP
    _heap_end = ORIGIN(REGION_KERNEL_HEAP) + LENGTH(REGION_KERNEL_HEAP);
