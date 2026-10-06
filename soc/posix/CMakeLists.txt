# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(include)
pbl_library_sources(
  core_dump.c
  memory.c
  soc.c
)

pbl_host_include_directories(include)
pbl_host_sources(host/main.c)

if(CONFIG_SDL)
  add_subdirectory(sdl)
endif()
