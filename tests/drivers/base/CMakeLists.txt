# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_device
  SOURCES
    drivers/base/device.c
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
)
