# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(led.c)
pbl_library_sources_ifdef(CONFIG_LED_AW2016 aw2016.c)
