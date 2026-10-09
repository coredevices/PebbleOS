/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    .pbl_input_callbacks : ALIGN(4) {
        __pbl_input_callbacks_start = .;
        KEEP(*(SORT_BY_NAME(.pbl_input_callbacks.*)))
        __pbl_input_callbacks_end = .;
    } >FLASH
