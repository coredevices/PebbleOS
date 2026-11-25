# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  init.c
  power_off.c
  ppi.c
)
if(CONFIG_SOC_NRF52_RETAINED_RAM)
  pbl_library_sources(retained.c)
  pbl_linker_sources(memory linker.ld)
endif()
