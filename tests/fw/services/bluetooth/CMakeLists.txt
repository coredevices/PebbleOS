# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ble_hrm
  SOURCES
    fw/services/bluetooth/ble_hrm.c
    tests/fakes/fake_events.c
  DEFINES CONFIG_HRM
)

pbl_clar_test(test_local_addr
  SOURCES
    fw/services/bluetooth/local_addr.c
)

pbl_clar_test(test_bluetooth_persistent_storage_prf
  SOURCES
    fw/services/bluetooth/bluetooth_persistent_storage_prf.c
    fw/services/shared_prf_storage/shared_prf_storage.c
    tests/fakes/fake_flash_region.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_events.c
  PLATFORMS obelix
  OVERRIDES shared_prf_storage_v3
)

pbl_clar_test(test_bluetooth_persistent_storage
  SOURCES
    fw/services/bluetooth/bluetooth_persistent_storage_normal.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/services/filesystem/pfs.c
    fw/services/filesystem/flash_translation.c
    fw/flash_region/flash_region.c
    fw/flash_region/filesystem_regions.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/system/hexdump.c
    tests/fakes/fake_shared_prf_storage.c
    tests/fakes/fake_events.c
    tests/fakes/fake_spi_flash.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board bluetooth_persistent_storage_v2
)
