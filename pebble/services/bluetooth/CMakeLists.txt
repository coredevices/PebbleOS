# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  advertising.c
  bluetooth_ctl.c
  bonding.c
  dis.c
  gateway.c
  link.c
  local_id.c
  pairability.c
  pp_ble_control.c
)
pbl_library()
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_BT_SERVICE_BAS battery_level.c)
pbl_library_sources_ifdef(CONFIG_SETTINGS bt_settings.c)
pbl_library_sources_ifdef(CONFIG_SHELL shell.c)
