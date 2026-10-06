# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_MCU_NRF5)
  list(APPEND sources nrf5.c)
elseif(CONFIG_MCU_SF32LB)
  list(APPEND sources sf32lb.c)
elseif(CONFIG_MCU_QEMU)
  list(APPEND sources qemu.c)
endif()
pbl_library()
pbl_library_sources(${sources})
