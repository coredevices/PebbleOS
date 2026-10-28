# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(audio.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_AMP_GPIO amp_gpio.c)
pbl_library_sources_ifdef(CONFIG_AUDIO_SHELL shell.c)
