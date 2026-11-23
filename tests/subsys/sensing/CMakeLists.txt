# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_sensing
  SOURCES
    drivers/base/device.c
    drivers/sensor/sensor.c
    subsys/sensing/sensing.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_SENSOR_LOG_LEVEL=0
    CONFIG_SENSING_LOG_LEVEL=0
    CONFIG_SENSING_SENSORS=2
    CONFIG_SENSING_BUFFER_FRAMES=40
  OVERRIDES dummy_board
)
