# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_health
  SOURCES
    fw/applib/health_service.c
    fw/process_management/pebble_process_info.c
    fw/util/time/mktime.c
    fw/util/time/time.c
    fw/util/stats.c
    tests/fakes/fake_rtc.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=1
  OVERRIDES dummy_board
)
