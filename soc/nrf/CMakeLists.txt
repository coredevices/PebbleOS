# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_NRF52)
  add_subdirectory(nrf52)
endif()
if(CONFIG_SOC_NRF54L)
  add_subdirectory(nrf54l)
endif()
if(CONFIG_CPU_NORDIC_VPR)
  add_subdirectory(vpr)
endif()
