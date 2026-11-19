# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_bonding
  SOURCES
    pebble/services/bluetooth/bonding.c
    third_party/nimble/store/prf.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS emery
)

pbl_clar_test(test_bt_settings
  MODULES settings
  SOURCES
    pebble/services/bluetooth/bt_settings_normal.c
    tests/fakes/fake_events.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_prf_store
  SOURCES
    third_party/nimble/store/prf.c
    tests/fakes/fake_spi_flash.c
  PLATFORMS emery
)
