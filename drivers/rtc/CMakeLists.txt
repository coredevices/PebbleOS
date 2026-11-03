# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(rtc.c)
pbl_library_sources_ifdef(CONFIG_RTC_RETAINED retained.c)
pbl_library_sources_ifdef(CONFIG_RTC_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_RTC_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_RTC_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_RTC_SHELL shell.c)
