/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_log_backends : ALIGN(4) {
        __pbl_log_backends_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_log_backends.*)))
        __pbl_log_backends_end = .;
    } >FLASH
