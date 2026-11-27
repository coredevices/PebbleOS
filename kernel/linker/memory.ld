/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * Memory regions, derived from the Kconfig memory layout symbols, which
 * default to the devicetree's memory and reserved memory nodes, or are set by
 * the SoC or board.
 *
 * The kernel's RAM runs from CONFIG_SRAM_BASE to __kernel_ram_end__, the end
 * of SRAM unless the application reserves RAM after the kernel's from a
 * memory linker snippet.
 */

MEMORY
{
#ifdef CONFIG_XIP
    /* CONFIG_FIRMWARE_OFFSET reserves room for the image header. */
    FLASH (rx) : ORIGIN = CONFIG_FLASH_BASE + CONFIG_FLASH_OFFSET + CONFIG_FIRMWARE_OFFSET,
                 LENGTH = CONFIG_FW_FLASH_SIZE - CONFIG_FIRMWARE_OFFSET
#endif

    RAM (xrw) : ORIGIN = CONFIG_SRAM_BASE,
                LENGTH = CONFIG_SRAM_SIZE
}

#ifndef CONFIG_XIP
/* The image runs from RAM, where it is copied to from its flash partition: code goes first. */
REGION_ALIAS("FLASH", RAM);
#endif

__FLASH_start__ = CONFIG_FLASH_BASE;
__FLASH_size__ = CONFIG_FLASH_SIZE;

__SRAM_size__ = CONFIG_SRAM_SIZE;

PROVIDE(__kernel_ram_end__ = ORIGIN(RAM) + LENGTH(RAM));

__RAM_start__ = ORIGIN(RAM);
__RAM_size__ = __kernel_ram_end__ - __RAM_start__;

REGION_ALIAS("REGION_ISR_STACK", RAM);
REGION_ALIAS("REGION_KERNEL_HEAP", RAM);
