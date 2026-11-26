# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(gpio.c)
pbl_library_sources_ifdef(CONFIG_GPIO_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_GPIO_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_GPIO_NRF54L nrf54l.c)
pbl_library_sources_ifdef(CONFIG_GPIO_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_GPIO_STM32 stm32.c)
pbl_library_sources_ifdef(CONFIG_GPIO_NPM1300 npm1300.c)
pbl_library_sources_ifdef(CONFIG_GPIO_TCA6408 tca6408.c)
pbl_library_sources_ifdef(CONFIG_GPIO_SHELL shell.c)
