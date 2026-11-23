# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_host_include_directories(.)
pbl_host_sources(sdl_bottom.c)

if(EMSCRIPTEN)
  # Emscripten's own SDL port, drawing to the page's canvas.
  pbl_host_compile_options(-sUSE_SDL=2)
  pbl_link_options(-sUSE_SDL=2)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  pbl_host_compile_options(${SDL2_CFLAGS})
  pbl_link_options(${SDL2_LDFLAGS})
endif()
