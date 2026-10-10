/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_devices : ALIGN(4) {
        __pbl_devices_start = .;
        KEEP(*(.pbl_devices))
        __pbl_devices_end = .;
    } >FLASH
