/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * Memory the CPU does not cache (PBL_NOCACHE), which one MPU region covers.
 * It comes first in RAM, whose start is aligned enough for an ARMv7-M
 * region. Zeroed at boot.
 */

    .kernel_nocache (NOLOAD) : ALIGN(32) {
        __nocache_start = .;
        *(.nocache .nocache.*)
        . = __nocache_start + PBL_MPU_REGION_SIZE(. - __nocache_start);
        __nocache_end = .;
    } >RAM
    __nocache_size = __nocache_end - __nocache_start;
    ASSERT(__nocache_start % PBL_MPU_REGION_ALIGN(__nocache_size) == 0,
           "The non-cacheable section is not aligned for its MPU region")
