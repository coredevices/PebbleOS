# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_BUTTON_NRF5)
  list(APPEND sources nrf5.c nrf5_debounced.c)
elseif(CONFIG_BUTTON_SF32LB)
  list(APPEND sources sf32lb.c sf32lb_debounced.c)
elseif(CONFIG_BUTTON_QEMU)
  list(APPEND sources qemu.c)
elseif(CONFIG_BUTTON_SDL)
  list(APPEND sources sdl/button_sdl.c)
  pbl_host_sources(sdl/button_sdl_bottom.c)
endif()
pbl_library()
pbl_library_sources(${sources})
