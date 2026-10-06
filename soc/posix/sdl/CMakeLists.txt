# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

pbl_host_include_directories(.)
pbl_host_compile_options(${SDL2_CFLAGS})
pbl_host_sources(sdl_bottom.c)
pbl_link_options(${SDL2_LDFLAGS})
