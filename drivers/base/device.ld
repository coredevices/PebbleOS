/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_devices : ALIGN(4) {
        __pbl_devices_start = .;
        KEEP(*(.pbl_devices))
        __pbl_devices_end = .;
    } >FLASH

    .pbl_dev_deps : ALIGN(4) {
        __pbl_dev_deps_start = .;
        KEEP(*(.pbl_dev_deps))
        __pbl_dev_deps_end = .;
    } >FLASH
