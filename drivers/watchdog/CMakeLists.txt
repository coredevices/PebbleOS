# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_WATCHDOG_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_WATCHDOG_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_WATCHDOG_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_WATCHDOG_STM32_IWDG stm32_iwdg.c)
