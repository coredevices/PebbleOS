# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
# The profiler is Cortex-M specific.
if(CONFIG_ARCH_POSIX)
  pbl_filter_sources(sources EXCLUDE profiler_syscalls.c)
endif()
if(NOT CONFIG_SYSCALL_FUZZ)
  pbl_filter_sources(sources EXCLUDE syscall_fuzz.c)
endif()
pbl_library_sources(${sources})
