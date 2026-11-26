# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_RNG_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_RNG_NRF_CRACEN nrf_cracen.c)
pbl_library_sources_ifdef(CONFIG_RNG_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_RNG_STM32 stm32.c)
pbl_library_sources_ifdef(CONFIG_RNG_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_RNG_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_RNG_SHELL shell.c)
