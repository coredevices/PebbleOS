# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_health
  SOURCES
    fw/applib/health_service.c
    fw/process_management/pebble_process_info.c
    lib/c/time/mktime.c
    fw/services/clock/time.c
    lib/c/time/gmtime.c
    tests/fakes/fake_rtc.c
  DEFINES CAPABILITY_HAS_HEALTH_TRACKING=1
  OVERRIDES dummy_board
)
