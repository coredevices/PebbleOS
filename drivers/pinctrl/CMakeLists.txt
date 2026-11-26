# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(pinctrl.c)
pbl_library_sources_ifdef(CONFIG_PINCTRL_SF32LB52 sf32lb52.c)
pbl_library_sources_ifdef(CONFIG_PINCTRL_SF32LB57 sf32lb57.c)
pbl_library_sources_ifdef(CONFIG_PINCTRL_STM32 stm32.c)
pbl_library_sources_ifdef(CONFIG_PINCTRL_SHELL shell.c)
