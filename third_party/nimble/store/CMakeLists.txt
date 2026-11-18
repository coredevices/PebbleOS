# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_NIMBLE_BLE_STORE_CONFIG_PERSIST)
  pbl_library()
  pbl_library_sources(settings.c)
  pbl_library_include_directories(${nimble}/nimble/host/store/config/src)
endif()
