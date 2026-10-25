/*
 * SPDX-FileCopyrightText: 2026 Core Devices LLC
 * SPDX-License-Identifier: Apache-2.0
 */

/* Zephyr iterable sections in flash (STRUCT_SECTION_ITERABLE of const objects) */
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
    } >FLASH
