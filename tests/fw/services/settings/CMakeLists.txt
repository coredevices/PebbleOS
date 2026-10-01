# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_settings_file
  SOURCES
    fw/applib/dict.c
    fw/services/filesystem/flash_translation.c
    fw/services/filesystem/pfs.c
    tests/fakes/fake_spi_flash.c
    fw/util/crc8.c
    fw/util/legacy_checksum.c
    fw/flash_region/filesystem_regions.c
    fw/flash_region/flash_region.c
    tests/fakes/fake_rtc.c
    fw/system/hexdump.c
    fw/services/settings/settings_file.c
    fw/services/settings/settings_raw_iter.c
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)
