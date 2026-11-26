# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_MFD_NPM1300)
  list(APPEND sources npm1300.c)
endif()
if(CONFIG_MFD_NRF_CRACEN)
  list(APPEND sources nrf_cracen.c)
endif()
pbl_library()
pbl_library_sources(${sources})
