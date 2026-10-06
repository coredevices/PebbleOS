# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

file(GLOB_RECURSE sources CONFIGURE_DEPENDS *.c)
list(REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/hpke.c)

pbl_library_named(libutil)
pbl_library_kind(stlib)
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_MBEDTLS hpke.c)
pbl_library_include_directories(.)
