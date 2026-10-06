# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
# Privilege switching is Cortex-M specific; so is the profiler.
if(CONFIG_ARCH_POSIX)
  pbl_filter_sources(sources EXCLUDE profiler_syscalls.c syscall_internal.c)
  list(APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/posix/syscall_internal.c)
endif()
pbl_library_sources(${sources})
