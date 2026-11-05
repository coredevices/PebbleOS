# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library_sources(
  arch.c
  cache.c
  debug.c
  fpu.c
  interrupts.c
  irq.c
  mpu.c
  reset.c
  vector_table.c
)
pbl_library_sources_ifdef(CONFIG_USERSPACE syscall.c)
pbl_library_sources_ifdef(CONFIG_ARMV7_M mpu_armv7m.c)
pbl_library_sources_ifdef(CONFIG_ARMV8_M_MAINLINE mpu_armv8m.c)

pbl_include_directories(include)
pbl_linker_include_directories(linker)
pbl_linker_sources(rom-start irq.ld)
