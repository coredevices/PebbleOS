# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  init.c
  lpsys.c
  mpu_regions.c
  power_off.c
)
pbl_library_sources_ifdef(CONFIG_CPU_IDLE idle.c)
