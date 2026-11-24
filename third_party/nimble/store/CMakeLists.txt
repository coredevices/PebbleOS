# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_NIMBLE_STORE_BACKEND_SETTINGS OR CONFIG_NIMBLE_STORE_PRF)
  pbl_library()
  pbl_library_sources_ifdef(CONFIG_NIMBLE_STORE_BACKEND_SETTINGS settings.c)
  pbl_library_sources_ifdef(CONFIG_NIMBLE_STORE_BACKEND_PRF prf_backend.c)
  pbl_library_sources_ifdef(CONFIG_NIMBLE_STORE_PRF_LAYOUT_LEGACY prf_legacy.c)
  pbl_library_sources_ifdef(CONFIG_NIMBLE_STORE_PRF_LAYOUT_RECORDS prf_records.c)
  pbl_library_include_directories(${nimble}/nimble/host/store/config/src)
endif()
