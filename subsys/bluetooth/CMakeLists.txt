# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(excludes)
if(NOT CONFIG_BT_GH3X2X_TUNING_SERVICE)
  list(APPEND excludes gh3x2x_tuning_service.c)
endif()
if(NOT CONFIG_BT_ID_ADDR_SF32LB52_EFUSE)
  list(APPEND excludes id_addr_sf32lb52.c)
endif()
if(NOT CONFIG_BT_ID_ADDR_STORAGE)
  list(APPEND excludes id_addr_storage.c)
endif()
file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_filter_sources(sources EXCLUDE ${excludes})
pbl_library()
pbl_library_sources(${sources})
