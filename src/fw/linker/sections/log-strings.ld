/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

    /*
     * Log strings for the console dehasher. Never loaded: the address is
     * LOG_STRINGS_SECTION_ADDRESS and the size is bounded by the 19-bit
     * section offset in the log token.
     */
    .log_strings 0xC0000000 (INFO) : {
        KEEP(*(.log_string.header))
        KEEP(*(.log_strings))
    }
    ASSERT(SIZEOF(.log_strings) <= 512K, "Log strings exceed the 512K token offset range")
