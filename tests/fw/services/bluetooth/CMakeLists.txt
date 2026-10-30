# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ble_hrm
  SOURCES
    fw/services/bluetooth/ble_hrm.c
    tests/fakes/fake_events.c
  DEFINES CONFIG_HRM
)

pbl_clar_test(test_bt_settings
  MODULES settings
  SOURCES
    fw/services/bluetooth/bt_settings_normal.c
    fw/services/bluetooth/bt_settings_sprf.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
)
