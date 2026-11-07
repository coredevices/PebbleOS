# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(coredump.c)
pbl_library_sources_ifdef(CONFIG_COREDUMP_BACKEND_FLASH backends/flash.c)
pbl_linker_sources(rom-start coredump.ld)
