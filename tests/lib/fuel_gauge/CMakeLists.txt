# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_fuel_gauge
  SOURCES
    lib/fuel_gauge/fuel_gauge.c
    lib/fuel_gauge/thevenin.c
  DEFINES
    CONFIG_FUEL_GAUGE_THEVENIN
)
