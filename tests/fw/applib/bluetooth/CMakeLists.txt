# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ble_ad_parse
  SOURCES
    fw/applib/bluetooth/ble_ad_parse.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    fw/system/hexdump.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_ble_ibeacon
  SOURCES
    fw/applib/bluetooth/ble_ibeacon.c
    fw/applib/bluetooth/ble_ad_parse.c
    fw/util/rand/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
    tests/fakes/fake_rtc.c
)
