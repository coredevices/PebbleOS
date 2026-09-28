# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(fuel_gauge.c)
pbl_library_sources_ifdef(CONFIG_FUEL_GAUGE_THEVENIN thevenin.c)
