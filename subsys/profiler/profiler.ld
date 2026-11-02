/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_profiler_nodes : ALIGN(4) {
        __pbl_profiler_nodes_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_profiler_nodes.*)))
        __pbl_profiler_nodes_end = .;
    } >FLASH
