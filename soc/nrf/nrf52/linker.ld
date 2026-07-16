/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

MEMORY
{
    RETAINED (xrw) : ORIGIN = 0x20000000, LENGTH = 256
}

SECTIONS
{
    .retained : ALIGN(4) {
        __retained_start = .;
        *(.retained)
        __retained_end = .;
    } > RETAINED
}
