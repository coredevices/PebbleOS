# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(touch.c)
pbl_library_sources_ifdef(CONFIG_TOUCH_CST816 cst816/cst816.c)
pbl_library_sources_ifdef(CONFIG_TOUCH_QEMU qemu.c)
if(CONFIG_TOUCH_SDL)
  pbl_library_sources(sdl/touch_sdl.c)
  pbl_host_sources(sdl/touch_sdl_bottom.c)
endif()
pbl_library_sources_ifdef(CONFIG_TOUCH_SHELL shell.c)
