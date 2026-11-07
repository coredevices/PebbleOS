# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  init.c
  reset.c
  sleep.c
  syscfg.c
  idle.c
  lpsys.c
  lpsys_halt.c
  mpu_regions.c
)
pbl_linker_sources(ramfunc ramfunc.ld)
