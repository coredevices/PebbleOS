# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_LOG_BACKEND_PULSE backends/pulse.c)
pbl_library_sources_ifdef(CONFIG_LOG_BACKEND_UART backends/uart.c)
pbl_library_sources_ifdef(CONFIG_LOG_BACKEND_FLASH
  backends/flash.c
  backends/flash_shell.c
  backends/flash_storage.c
)
pbl_linker_sources(rom-start backends.ld)
