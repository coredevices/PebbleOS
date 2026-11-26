# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT OR CONFIG_BT_GATT_SERVER)
  pbl_library()
  pbl_library_sources_ifdef(CONFIG_BT services.c)
  pbl_library_sources_ifdef(CONFIG_BT_GATT_SERVER gatts.c)
endif()

pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_BAS bas)
pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_DIS dis)
pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_PPOG_REVERSED ppog_reversed)
pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_PPS pps)
