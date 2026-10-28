# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(audio.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_AMP_GPIO amp_gpio.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_SF32LB52_AUDCODEC sf32lb52_audcodec.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_SF32LB52_PDM sf32lb52_pdm.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_HOST_MIC host_mic.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_NRF5_PDM nrf5_pdm.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_NRF5_I2S nrf5_i2s.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_DA7212 da7212.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_SHELL shell.c)
