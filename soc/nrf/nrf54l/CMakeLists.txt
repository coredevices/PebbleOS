# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
if(CONFIG_SOC_NRF54L_CPUAPP)
  pbl_library_sources(
    init.c
    power_off.c
  )
else()
  pbl_library_sources(flpr.c)
endif()
