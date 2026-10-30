# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  bluetooth_ctl.c
  bonding.c
  bt_settings_sprf.c
  dis.c
  local_id.c
  pairability.c
  pebble_pairing_service.c
  pp_ble_control.c
)
if(NOT CONFIG_RECOVERY_FW)
  list(APPEND sources bt_settings_normal.c)
else()
  list(APPEND sources bt_settings_prf.c)
endif()
pbl_library()
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_BT_SERVICE_BAS battery_level.c)
pbl_library_sources_ifdef(CONFIG_SERVICE_BLE_HRM ble_hrm.c)
pbl_library_sources_ifdef(CONFIG_SHELL shell.c)
