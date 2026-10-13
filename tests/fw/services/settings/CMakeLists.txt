# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_settings_file
  MODULES settings rand
  SOURCES
    fw/applib/dict.c
    tests/fakes/fake_rtc.c
    fw/system/hexdump.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)
