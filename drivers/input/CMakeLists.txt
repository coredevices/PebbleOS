# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_INPUT_ADC_KEYS adc_keys.c)
pbl_library_sources_ifdef(CONFIG_INPUT_GPIO_KEYS gpio_keys.c)
pbl_library_sources_ifdef(CONFIG_INPUT_ROTARY_ENCODER rotary_encoder.c)
pbl_library_sources_ifdef(CONFIG_INPUT_SDL sdl/input_sdl.c)
pbl_host_sources_ifdef(CONFIG_INPUT_SDL sdl/input_sdl_bottom.c)
