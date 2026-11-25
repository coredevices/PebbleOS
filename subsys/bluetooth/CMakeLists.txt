# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(clients)
pbl_add_subdirectory_ifdef(CONFIG_BT_PRF_STORE prf_store)

if(CONFIG_BT)
  file(GLOB sources CONFIGURE_DEPENDS *.c)
  pbl_library()
  pbl_library_sources(${sources})

  add_subdirectory(services)
endif()
