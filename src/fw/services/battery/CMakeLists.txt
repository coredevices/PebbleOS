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
  if(CONFIG_BOARD_ASTERIX)
    list(APPEND sources fuel_gauge/models/asterix.c)
  elseif(CONFIG_BOARD_OBELIX)
    list(APPEND sources fuel_gauge/models/obelix.c)
  elseif(CONFIG_BOARD_GETAFIX)
    list(APPEND sources fuel_gauge/models/getafix.c)
  else()
    message(FATAL_ERROR "no fuel gauge model for board ${PBL_BOARD}")
  endif()
else()
  list(APPEND sources
    voltage/battery_curve.c
    voltage/battery_state.c
  )
endif()
pbl_library()
pbl_library_sources(${sources})
