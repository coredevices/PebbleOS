# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_add_subdirectory_ifdef(CONFIG_BT_ADV adv)
add_subdirectory(clients)
add_subdirectory(services)
pbl_add_subdirectory_ifdef(CONFIG_BT_PRF_STORE prf_store)

if(CONFIG_BT)
  file(GLOB sources CONFIGURE_DEPENDS *.c)
  pbl_library()
  pbl_library_sources(${sources})
endif()
