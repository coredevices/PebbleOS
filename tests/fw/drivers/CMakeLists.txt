# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_flash_api
  SOURCES
    fw/drivers/flash/flash_api.c
  OVERRIDES dummy_board
)

pbl_clar_test(test_flash_erase
  SOURCES
    fw/drivers/flash/flash_erase.c
  PLATFORMS obelix
  OVERRIDES dummy_board
)
