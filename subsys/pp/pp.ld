/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_pp_endpoints : ALIGN(4) {
        __pbl_pp_endpoints_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_pp_endpoints.*)))
        __pbl_pp_endpoints_end = .;
    } >FLASH

    .pbl_pp_session_listeners : ALIGN(4) {
        __pbl_pp_session_listeners_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_pp_session_listeners.*)))
        __pbl_pp_session_listeners_end = .;
    } >FLASH
