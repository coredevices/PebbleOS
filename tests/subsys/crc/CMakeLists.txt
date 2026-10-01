# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_crc
  SOURCES
    subsys/crc/crc8.c
    subsys/crc/crc32.c
    subsys/crc/crc32_legacy.c
)

pbl_clar_test(test_crc_hw
  SOURCES
    subsys/crc/crc32.c
    subsys/crc/crc32_legacy.c
  DEFINES
    CONFIG_CRC_HW_ACCEL=1
    CONFIG_CRC_HW_MIN_LEN=16
)

pbl_clar_test(test_crc32)

pbl_clar_test(test_crc32_legacy)
