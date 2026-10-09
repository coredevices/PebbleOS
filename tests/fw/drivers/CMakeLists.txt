# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_flash_api
  SOURCES
    drivers/flash/flash_api.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_flash_erase
  SOURCES
    drivers/flash/flash_erase.c
  PLATFORMS obelix
  OVERRIDES dummy_board
)

pbl_clar_test(test_accel_qemu
  SOURCES
    drivers/imu/qemu.c
    tests/fakes/fake_rtc.c
  OVERRIDES dummy_board
)
