# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(timer.c)
pbl_library_sources_ifdef(CONFIG_TIMER_ARM_SYSTICK arm_systick.c)
pbl_library_sources_ifdef(CONFIG_TIMER_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_TIMER_NRF5_RTC nrf5_rtc.c)
pbl_library_sources_ifdef(CONFIG_TIMER_SF32LB_LPTIM sf32lb_lptim.c)
pbl_library_sources_ifdef(CONFIG_TIMER_POSIX posix.c)
