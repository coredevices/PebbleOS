# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(nvmem.c)
pbl_library_sources_ifdef(CONFIG_NVMEM_SF32LB52 sf32lb52.c)
pbl_library_sources_ifdef(CONFIG_NVMEM_SHELL shell.c)
