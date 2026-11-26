# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(regulator.c)
pbl_library_sources_ifdef(CONFIG_REGULATOR_NRF54L nrf54l.c)
pbl_library_sources_ifdef(CONFIG_REGULATOR_STM32H7_PWR stm32h7_pwr.c)
pbl_library_sources_ifdef(CONFIG_REGULATOR_FIXED fixed.c)
pbl_library_sources_ifdef(CONFIG_REGULATOR_NPM1300 npm1300.c)
pbl_library_sources_ifdef(CONFIG_REGULATOR_SF32LB52_PMUC sf32lb52_pmuc.c)
pbl_library_sources_ifdef(CONFIG_REGULATOR_SF32LB57_PMUC sf32lb57_pmuc.c)
