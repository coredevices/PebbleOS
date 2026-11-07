# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_analytics_native
  SOURCES
    subsys/analytics/native.c
)

pbl_clar_test(test_analytics
  SOURCES
    subsys/analytics/analytics.c
  DEFINES
    PBL_NO_LINKER_SCRIPT=1
)
