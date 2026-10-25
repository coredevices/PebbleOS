/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

/* The Zephyr host's RAM, in the kernel's RAM but apart from its .data and
 * .bss: zbt_start() zeroes .zbt_wbss and copies the initial values of
 * .zbt_wdata in each time, so each start begins afresh. The control side
 * (zephyr_bt_ctl) is in the kernel's own sections.
 */
    .zbt_wdata : ALIGN(32) {
        __zbt_wdata_start = .;
        *CMakeFiles/zephyr_bt.dir* (.data .data.*)
        . = ALIGN(4);
        _net_buf_pool_list_start = .;
        KEEP(*(SORT_BY_NAME(._net_buf_pool.static.*)))
        _net_buf_pool_list_end = .;
        . = ALIGN(4);
        __zbt_wdata_end = .;
    } >RAM AT>FLASH
    __zbt_wdata_load_start = LOADADDR(.zbt_wdata);

    .zbt_wbss (NOLOAD) : ALIGN(8) {
        __zbt_wbss_start = .;
        *CMakeFiles/zephyr_bt.dir* (.bss .bss.* COMMON)
        . = ALIGN(4);
        __zbt_wbss_end = .;
    } >RAM
