# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(nvmem.c)
if(CONFIG_NVMEM_SF32LB52 OR CONFIG_NVMEM_SF32LB57)
  pbl_library_sources(sf32lb52.c)
endif()
pbl_library_sources_ifdef(CONFIG_NVMEM_SHELL shell.c)
