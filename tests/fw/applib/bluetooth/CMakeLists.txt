# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ble_ad_parse
  MODULES rand
  SOURCES
    fw/applib/bluetooth/ble_ad_parse.c
    fw/system/hexdump.c
    tests/fakes/fake_rtc.c
)
