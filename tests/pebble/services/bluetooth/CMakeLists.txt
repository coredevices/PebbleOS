# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_bonding
  SOURCES
    pebble/services/bluetooth/bonding.c
    subsys/bluetooth/prf_store/legacy.c
    tests/fakes/fake_spi_flash.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    subsys/bluetooth/prf_store
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  PLATFORMS emery
)

# The same, over the other layout of the PRF store.
pbl_clar_test(test_bonding_records
  SOURCE test_bonding.c
  SOURCES
    pebble/services/bluetooth/bonding.c
    subsys/bluetooth/prf_store/records.c
    tests/fakes/fake_spi_flash.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    subsys/bluetooth/prf_store
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  PLATFORMS emery
)

pbl_clar_test(test_pairability
  SOURCES
    pebble/services/bluetooth/pairability.c
    tests/fakes/fake_rtc.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  PLATFORMS emery
)

pbl_clar_test(test_prf_backend
  SOURCES
    third_party/nimble/store/prf_backend.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/host/store/config/src
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  DEFINES
    CONFIG_NIMBLE_LOG_LEVEL=0
    CONFIG_NIMBLE_BLE_STORE_CONFIG_PERSIST=1
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
    subsys/bluetooth/prf_store/legacy.c
    tests/fakes/fake_spi_flash.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    subsys/bluetooth/prf_store
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  PLATFORMS emery
)

pbl_clar_test(test_prf_store_records
  SOURCES
    subsys/bluetooth/prf_store/records.c
    tests/fakes/fake_spi_flash.c
    tests/stubs/stubs_nimble_port.c
  INCLUDES
    subsys/bluetooth/prf_store
    third_party/nimble/port/include
    third_party/nimble/mynewt-nimble/nimble/include
    third_party/nimble/mynewt-nimble/nimble/host/include
    third_party/nimble/mynewt-nimble/nimble/transport/include
    third_party/nimble/mynewt-nimble/porting/nimble/include
  PLATFORMS emery
)
