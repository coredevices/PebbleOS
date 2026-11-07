/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_coredump_backends : ALIGN(4) {
        __pbl_coredump_backends_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_coredump_backends.*)))
        __pbl_coredump_backends_end = .;
    } >FLASH

    .pbl_coredump_regions : ALIGN(4) {
        __pbl_coredump_regions_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_coredump_regions.*)))
        __pbl_coredump_regions_end = .;
    } >FLASH
