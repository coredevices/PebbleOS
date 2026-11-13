# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ram
  SOURCES
    drivers/base/device.c
    drivers/ram/ram.c
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
)
