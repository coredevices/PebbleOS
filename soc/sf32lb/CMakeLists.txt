# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(include)
pbl_library_sources(sleep.c)

if(CONFIG_SOC_SF32LB52)
  add_subdirectory(sf32lb52x)
endif()
