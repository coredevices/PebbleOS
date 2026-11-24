/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .kernel_data : ALIGN(32) {
        __data_start = .; /* This is used by the startup in order to initialize the .data section */

        *(.data)
        *(.data.*)

        . = ALIGN(8);
        __data_end = .;
    } >RAM AT>FLASH
    __data_load_start = LOADADDR(.kernel_data);

    .kernel_ro_bss (NOLOAD) : {
        __bss_start = .; /* This is used by the startup in order to initialize the .bss section */

#ifdef CONFIG_USERSPACE
        . = ALIGN(__unpriv_ro_bss_size__);
#endif
        __unpriv_ro_bss_start__ = .;

        *:lib_a-timelocal.o(.bss.*)
        *:lib_a-tzvars.o(.bss.*)

        *(.kernel_unpriv_ro_bss)

#ifdef CONFIG_USERSPACE
        . = __unpriv_ro_bss_start__ + __unpriv_ro_bss_size__;
#endif
    } >RAM

    .kernel_bss (NOLOAD) : {
        *(.freertos_privileged_data)

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
