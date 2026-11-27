# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_display_class
  SOURCES
    drivers/base/device.c
    drivers/display/display.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_DISPLAY_LOG_LEVEL=0
    CONFIG_DISPLAY_MAX_UPDATE_ROWS=24
    CONFIG_DISPLAY_UPDATE_TIMEOUT_MS=200
)
