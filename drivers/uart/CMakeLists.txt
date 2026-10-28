# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(uart.c)
pbl_library_sources_ifdef(CONFIG_UART_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_UART_POSIX posix/uart_posix.c)
pbl_library_sources_ifdef(CONFIG_UART_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_UART_SF32LB sf32lb.c)
if(CONFIG_UART_POSIX)
  pbl_host_sources(posix/uart_posix_bottom.c)
endif()
