# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_audio
  SOURCES
    drivers/base/device.c
    drivers/audio/audio.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_AUDIO_LOG_LEVEL=0
)

pbl_clar_test(test_nrf5_i2s
  SOURCES
    drivers/base/device.c
    drivers/audio/audio.c
    drivers/audio/nrf5_i2s.c
  INCLUDES
    soc/nrf/nrf52
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_AUDIO_LOG_LEVEL=0
    CONFIG_AUDIO_NRF5_I2S_BLOCK_FRAMES=8
)
