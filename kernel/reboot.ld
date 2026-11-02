/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_sys_off_handlers : ALIGN(4) {
        __pbl_sys_off_handlers_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_sys_off_handlers.*)))
        __pbl_sys_off_handlers_end = .;
    } >FLASH
