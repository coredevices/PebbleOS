# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources battery_monitor.c)
if(CONFIG_NRF_FUEL_GAUGE)
  list(APPEND sources
    nrf_fuel_gauge/battery_curve.c
    nrf_fuel_gauge/battery_state.c
  )
elseif(CONFIG_BATTERY_SOC_FUEL_GAUGE)
  list(APPEND sources
    fuel_gauge/battery_curve.c
    fuel_gauge/battery_state.c
  )
  set(model fuel_gauge/models/${CONFIG_BATTERY_SOC_FUEL_GAUGE_MODEL}.c)
  if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${model})
    message(FATAL_ERROR "no fuel gauge cell model ${CONFIG_BATTERY_SOC_FUEL_GAUGE_MODEL}")
  endif()
  list(APPEND sources ${model})
else()
  list(APPEND sources
    voltage/battery_curve.c
    voltage/battery_state.c
  )
endif()
pbl_library()
pbl_library_sources(${sources})
