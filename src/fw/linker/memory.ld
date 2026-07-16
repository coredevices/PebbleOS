/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * Memory regions, derived from the Kconfig memory layout symbols
 * (defaulted per SoC/board in the Kconfig.defconfig files).
 *
 * SRAM is carved up sequentially from CONFIG_SRAM_BASE: the kernel gets
 * whatever is left after the worker and the app take their share at the end.
 * The app segment is sized for the largest app execution environment (2x, 3x
 * or 4x) supported by the platform.
 */

#define PBL_MAX(a, b) ((a) > (b) ? (a) : (b))

#define PBL_APP_RAM_2X_SIZE \
    (CONFIG_APP_RAM_2X_SEGMENT_SIZE + CONFIG_APP_RAM_2X_RUNTIME_SIZE)
#define PBL_APP_RAM_3X_SIZE \
    (CONFIG_APP_RAM_3X_SEGMENT_SIZE + CONFIG_APP_RAM_3X_RUNTIME_SIZE)
#define PBL_APP_RAM_4X_SIZE \
    (CONFIG_APP_RAM_4X_SEGMENT_SIZE + CONFIG_APP_RAM_4X_RUNTIME_SIZE)

#define PBL_APP_RAM_SIZE \
    PBL_MAX(PBL_APP_RAM_2X_SIZE, PBL_MAX(PBL_APP_RAM_3X_SIZE, PBL_APP_RAM_4X_SIZE))

/* The worker always gets 12k of RAM. */
#define PBL_WORKER_RAM_SIZE (12 * 1024)

#define PBL_KERNEL_RAM_SIZE \
    (CONFIG_SRAM_SIZE - PBL_WORKER_RAM_SIZE - PBL_APP_RAM_SIZE)

MEMORY
{
    /* CONFIG_FIRMWARE_OFFSET reserves room for the image header. */
    FLASH (rx) : ORIGIN = CONFIG_FLASH_BASE + CONFIG_FLASH_OFFSET + CONFIG_FIRMWARE_OFFSET,
                 LENGTH = CONFIG_FW_FLASH_SIZE - CONFIG_FIRMWARE_OFFSET

    KERNEL_RAM (xrw) : ORIGIN = CONFIG_SRAM_BASE,
                       LENGTH = PBL_KERNEL_RAM_SIZE
    WORKER_RAM (xrw) : ORIGIN = CONFIG_SRAM_BASE + PBL_KERNEL_RAM_SIZE,
                       LENGTH = PBL_WORKER_RAM_SIZE
    APP_RAM (xrw)    : ORIGIN = CONFIG_SRAM_BASE + PBL_KERNEL_RAM_SIZE + PBL_WORKER_RAM_SIZE,
                       LENGTH = PBL_APP_RAM_SIZE
}

__FLASH_start__ = CONFIG_FLASH_BASE;
__FLASH_size__ = CONFIG_FLASH_SIZE;

REGION_ALIAS("REGION_ISR_STACK", KERNEL_RAM);
REGION_ALIAS("REGION_KERNEL_STACKS", KERNEL_RAM);
REGION_ALIAS("REGION_KERNEL_HEAP", KERNEL_RAM);

/* The process loader enforces eight-byte alignment on all segments. */
ASSERT(CONFIG_APP_RAM_2X_SEGMENT_SIZE % 8 == 0,
       "The app_segment size for the 2x app environment is not a multiple of eight bytes");
ASSERT(CONFIG_APP_RAM_3X_SEGMENT_SIZE % 8 == 0,
       "The app_segment size for the 3x app environment is not a multiple of eight bytes");
ASSERT(CONFIG_APP_RAM_4X_SEGMENT_SIZE % 8 == 0,
       "The app_segment size for the 4x app environment is not a multiple of eight bytes");

/* Basic sanity checks: apps must fit, and the kernel needs at least 64k. */
ASSERT(PBL_APP_RAM_SIZE > 0, "App RAM is too small!")
ASSERT(PBL_KERNEL_RAM_SIZE >= 64K, "Kernel RAM is too small!")
