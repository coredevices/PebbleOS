/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * The firmware's Zephyr sections (third_party/zephyr_bt/linker/) for the host
 * simulation. The host's .data and .bss are apart here, not in one worker
 * RAM; host_sim.c keeps the initial values of .data in sim_wdata_image.
 */
SECTIONS {
    .zbt_rom_lists : ALIGN(8) {
        . = ALIGN(8);
        _bt_l2cap_fixed_chan_list_start = .;
        KEEP(*(SORT_BY_NAME(._bt_l2cap_fixed_chan.static.*)))
        _bt_l2cap_fixed_chan_list_end = .;
        . = ALIGN(8);
        _bt_l2cap_br_fixed_chan_list_start = .;
        KEEP(*(SORT_BY_NAME(._bt_l2cap_br_fixed_chan.static.*)))
        _bt_l2cap_br_fixed_chan_list_end = .;
        . = ALIGN(8);
        _bt_gatt_service_static_list_start = .;
        KEEP(*(SORT_BY_NAME(._bt_gatt_service_static.static.*)))
        _bt_gatt_service_static_list_end = .;
        . = ALIGN(8);
        _bt_conn_cb_list_start = .;
        KEEP(*(SORT_BY_NAME(._bt_conn_cb.static.*)))
        _bt_conn_cb_list_end = .;
        . = ALIGN(8);
        _bt_sco_conn_cb_list_start = .;
        KEEP(*(SORT_BY_NAME(._bt_sco_conn_cb.static.*)))
        _bt_sco_conn_cb_list_end = .;
        . = ALIGN(8);
        _bt_sco_hci_cb_list_start = .;
        KEEP(*(SORT_BY_NAME(._bt_sco_hci_cb.static.*)))
        _bt_sco_hci_cb_list_end = .;
        . = ALIGN(8);
        _zbt_init_entry_list_start = .;
        KEEP(*(SORT_BY_NAME(._zbt_init_entry.static.*)))
        _zbt_init_entry_list_end = .;
    }
} INSERT AFTER .rodata;
SECTIONS {
    .zbt_wdata : ALIGN(32) {
        __zbt_wdata_start = .;
        */obj/z_*.o(.data .data.*)
        . = ALIGN(4);
        _net_buf_pool_list_start = .;
        KEEP(*(SORT_BY_NAME(._net_buf_pool.static.*)))
        _net_buf_pool_list_end = .;
        . = ALIGN(4);
        __zbt_wdata_end = .;
    }
    __zbt_wdata_load_start = sim_wdata_image;
} INSERT AFTER .data;
SECTIONS {
    .zbt_wbss (NOLOAD) : ALIGN(8) {
        __zbt_wbss_start = .;
        */obj/z_*.o(.bss .bss.* COMMON)
        . = ALIGN(4);
        __zbt_wbss_end = .;
    }
} INSERT AFTER .bss;
