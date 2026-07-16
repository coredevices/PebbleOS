/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /* Unloaded section containing our log strings. */
    .log_strings (INFO) : {
        KEEP(*(.log_string.header))
        KEEP(*(.log_strings))
    } >LOG_STRINGS
