# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_coredump
  SOURCES
    subsys/coredump/coredump.c
    subsys/coredump/backends/flash.c
    tests/fakes/fake_spi_flash.c
  DEFINES DUMA_DISABLED PBL_NO_LINKER_SCRIPT CONFIG_COREDUMP_BACKEND_FLASH_IMAGES=2
  OVERRIDES dummy_board
)
