# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB_RECURSE sources CONFIGURE_DEPENDS *.c)
# Core dumps capture Cortex-M state; a native build has a debugger instead.
if(CONFIG_ARCH_POSIX)
  pbl_filter_sources(sources EXCLUDE core_dump.c)
endif()
pbl_library_sources(${sources})
