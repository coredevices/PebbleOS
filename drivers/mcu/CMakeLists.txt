# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_MCU_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_MCU_NRF54L nrf54l.c)
pbl_library_sources_ifdef(CONFIG_MCU_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_MCU_STM32 stm32.c)
pbl_library_sources_ifdef(CONFIG_MCU_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_MCU_POSIX posix.c)
