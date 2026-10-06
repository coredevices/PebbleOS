# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
# The profiler counts Cortex-M cycles.
if(NOT CONFIG_CPU_CORTEX_M)
  pbl_filter_sources(sources EXCLUDE profiler.c)
endif()
pbl_library_sources(${sources})
