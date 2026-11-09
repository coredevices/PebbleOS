# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_settings
  MODULES settings rand
  SOURCES
    pebble/applib/dict.c
    tests/fakes/fake_rtc.c
    subsys/logging/hexdump.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)
