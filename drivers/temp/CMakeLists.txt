# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(temp.c)
pbl_library_sources_ifdef(CONFIG_TEMP_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_TEMP_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_TEMP_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_TEMP_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_TEMP_SHELL shell.c)
