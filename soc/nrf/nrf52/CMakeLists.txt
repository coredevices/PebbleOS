# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  ${PBL_BASE}/third_party/hal_nordic/nrfx/mdk/system_nrf52840.c
  init.c
  sleep.c
  startup.c
  idle.c
)
pbl_linker_sources(memory linker.ld)
