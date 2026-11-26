# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(i2c.c)
pbl_library_sources_ifdef(CONFIG_I2C_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_I2C_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_I2C_STM32 stm32.c)
pbl_library_sources_ifdef(CONFIG_I2C_SHELL shell.c)
