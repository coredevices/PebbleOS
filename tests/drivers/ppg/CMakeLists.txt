# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_ppg
  SOURCES
    drivers/base/device.c
    drivers/ppg/ppg.c
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
)
