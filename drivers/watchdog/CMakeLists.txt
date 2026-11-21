# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_WATCHDOG_QEMU)
  list(APPEND sources qemu.c)
elseif(CONFIG_WATCHDOG_NRF5)
  list(APPEND sources nrf5.c)
elseif(CONFIG_WATCHDOG_SF32LB)
  list(APPEND sources sf32lb.c)
elseif(CONFIG_WATCHDOG_STM32_IWDG)
  list(APPEND sources stm32_iwdg.c)
endif()
pbl_library()
pbl_library_sources(${sources})
