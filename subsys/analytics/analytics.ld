/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_analytics_collectors : ALIGN(4) {
        __pbl_analytics_collectors_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_analytics_collectors.*)))
        __pbl_analytics_collectors_end = .;
    } >FLASH
