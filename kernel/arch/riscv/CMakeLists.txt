# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library_sources(
  arch.c
  trap.S
)
pbl_library_sources_ifdef(CONFIG_RISCV_CLIC clic.c)

pbl_include_directories(include)
pbl_linker_include_directories(linker)
pbl_iterable_section(irq_prio UNSORTED)
pbl_link_options(-Wl,--entry=arch_riscv_reset)
