# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_flash_region
  SOURCES
    fw/flash_region/flash_region.c
  PLATFORMS emery
  OVERRIDES dummy_board
)
