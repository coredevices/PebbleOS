# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(temperature.c)
pbl_library_sources_ifdef(CONFIG_TEMPERATURE_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_TEMPERATURE_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_TEMPERATURE_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_TEMPERATURE_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_TEMPERATURE_SHELL shell.c)
