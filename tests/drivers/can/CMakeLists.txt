# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_can_class
  SOURCES
    drivers/base/device.c
    drivers/can/can.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
)
