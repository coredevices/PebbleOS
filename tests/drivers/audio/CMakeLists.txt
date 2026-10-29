# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_audio
  SOURCES
    drivers/base/device.c
    drivers/audio/audio.c
  DEFINES
    PBL_NO_LINKER_SCRIPT
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_DRIVER_AUDIO_LOG_LEVEL=0
)
