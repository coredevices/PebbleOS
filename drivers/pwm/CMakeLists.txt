# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(pwm.c)
pbl_library_sources_ifdef(CONFIG_PWM_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_PWM_NRF5_RTC nrf5_rtc.c)
pbl_library_sources_ifdef(CONFIG_PWM_SF32LB_LPTIM sf32lb_lptim.c)
pbl_library_sources_ifdef(CONFIG_PWM_SHELL shell.c)
