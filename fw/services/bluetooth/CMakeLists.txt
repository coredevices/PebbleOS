# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  bluetooth_ctl.c
  bluetooth_persistent_storage_debug.c
  bonding.c
  dis.c
  local_addr.c
  local_id.c
  pairability.c
  pebble_pairing_service.c
  pp_ble_control.c
)
if(NOT CONFIG_RECOVERY_FW)
  list(APPEND sources bluetooth_persistent_storage_normal.c)
else()
  list(APPEND sources bluetooth_persistent_storage_prf.c)
endif()
pbl_library()
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_BT_SERVICE_BAS battery_level.c)
pbl_library_sources_ifdef(CONFIG_SERVICE_BLE_HRM ble_hrm.c)
pbl_library_sources_ifdef(CONFIG_SHELL shell.c)
