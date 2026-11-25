/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * The worker and app segments, at the end of RAM, after the kernel's. They
 * are not memory regions: the firmware overwrites them wholesale at runtime,
 * so nothing may ever be linked into them. Only their bounds are exported.
 */

#define PBL_MAX(a, b) ((a) > (b) ? (a) : (b))

#define PBL_APP_RAM_2X_SIZE \
    (CONFIG_APP_RAM_2X_SEGMENT_SIZE + CONFIG_APP_RAM_2X_RUNTIME_SIZE)
#define PBL_APP_RAM_3X_SIZE \
    (CONFIG_APP_RAM_3X_SEGMENT_SIZE + CONFIG_APP_RAM_3X_RUNTIME_SIZE)
#define PBL_APP_RAM_4X_SIZE \
    (CONFIG_APP_RAM_4X_SEGMENT_SIZE + CONFIG_APP_RAM_4X_RUNTIME_SIZE)

__APP_RAM_size__ =
    PBL_MAX(PBL_APP_RAM_2X_SIZE, PBL_MAX(PBL_APP_RAM_3X_SIZE, PBL_APP_RAM_4X_SIZE));
__APP_RAM_end__ = ORIGIN(RAM) + LENGTH(RAM);
__APP_RAM__ = __APP_RAM_end__ - __APP_RAM_size__;

__WORKER_RAM_size__ = CONFIG_WORKER_RAM_SEGMENT_SIZE + CONFIG_WORKER_RAM_RUNTIME_SIZE;
__WORKER_RAM_end__ = __APP_RAM__;
__WORKER_RAM__ = __WORKER_RAM_end__ - __WORKER_RAM_size__;

__kernel_ram_end__ = __WORKER_RAM__;

/* The process loader enforces eight-byte alignment on all segments. */
ASSERT(CONFIG_APP_RAM_2X_SEGMENT_SIZE % 8 == 0,
       "The app_segment size for the 2x app environment is not a multiple of eight bytes");
ASSERT(CONFIG_APP_RAM_3X_SEGMENT_SIZE % 8 == 0,
       "The app_segment size for the 3x app environment is not a multiple of eight bytes");
ASSERT(CONFIG_APP_RAM_4X_SEGMENT_SIZE % 8 == 0,
       "The app_segment size for the 4x app environment is not a multiple of eight bytes");
ASSERT(CONFIG_WORKER_RAM_SEGMENT_SIZE % 8 == 0,
       "The worker segment size is not a multiple of eight bytes");
