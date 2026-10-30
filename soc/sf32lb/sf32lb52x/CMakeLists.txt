# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  init.c
  sleep.c
  idle.c
)
pbl_library_sources_ifdef(CONFIG_SF32LB52_PSRAM psram.c)
pbl_linker_sources(ramfunc ramfunc.ld)
