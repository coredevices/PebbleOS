# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_reset
  SOURCES
    drivers/base/device.c
    drivers/reset/reset.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
)
