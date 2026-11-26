# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(clk.c)
pbl_library_sources_ifdef(CONFIG_CLK_SHELL shell.c)
pbl_library_sources_ifdef(CONFIG_CLK_FIXED fixed.c)
pbl_library_sources_ifdef(CONFIG_CLK_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_CLK_STM32H7 stm32h7.c)
pbl_library_sources_ifdef(CONFIG_CLK_NRF52 nrf52.c)
pbl_library_sources_ifdef(CONFIG_CLK_NRF54L nrf54l.c)
pbl_library_sources_ifdef(CONFIG_CLK_CAL cal.c)
pbl_library_sources_ifdef(CONFIG_CLK_SF32LB52_PMUC sf32lb52_pmuc.c)
pbl_library_sources_ifdef(CONFIG_CLK_SF32LB52_HXT sf32lb52_hxt.c)
pbl_library_sources_ifdef(CONFIG_CLK_SF32LB57_PMUC sf32lb57_pmuc.c)
pbl_library_sources_ifdef(CONFIG_CLK_SF32LB_LPSYS_RCC sf32lb_lpsys_rcc.c)
pbl_library_sources_ifdef(CONFIG_CLK_SF32LB52_RTC sf32lb52_rtc.c)
