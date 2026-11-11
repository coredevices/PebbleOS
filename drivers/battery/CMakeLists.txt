# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_BATTERY)
  list(APPEND sources battery_common.c)
endif()
if(CONFIG_BATTERY_SIM)
  list(APPEND sources sim.c)
endif()
if(CONFIG_BATTERY_SIMPLE)
  list(APPEND sources simple_battery.c)
endif()
pbl_library()
pbl_library_sources(${sources})
