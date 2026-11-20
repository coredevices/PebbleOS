# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BT_GATT_CLIENT)
  pbl_library()
  pbl_library_sources(gatt.c op_queue.c)
  pbl_iterable_section(bt_gattc_ntf)
endif()

pbl_add_subdirectory_ifdef(CONFIG_BT_AMS_CLIENT ams)
pbl_add_subdirectory_ifdef(CONFIG_BT_ANCS_CLIENT ancs)
