# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB_RECURSE sources CONFIGURE_DEPENDS *.c)
if(NOT CONFIG_COREDUMP)
  pbl_filter_sources(sources EXCLUDE core_dump_platform.c)
endif()
pbl_library_sources(${sources})
