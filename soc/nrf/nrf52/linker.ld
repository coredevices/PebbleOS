/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

MEMORY
{
    RETAINED (xrw) : ORIGIN = CONFIG_SOC_NRF52_RETAINED_RAM_BASE,
                     LENGTH = CONFIG_SOC_NRF52_RETAINED_RAM_SIZE
}

SECTIONS
{
    .retained : ALIGN(4) {
        __retained_start = .;
        *(.retained)
        __retained_end = .;
    } > RETAINED
}
