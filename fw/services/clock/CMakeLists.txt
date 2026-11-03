# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(rtc_sync.c service.c time.c)
pbl_library_sources_ifdef(CONFIG_SERVICE_CLOCK_TZ_STORAGE_FLASH tz_storage_flash.c)
pbl_library_sources_ifdef(CONFIG_SERVICE_CLOCK_TZ_STORAGE_RETAINED tz_storage_retained.c)
