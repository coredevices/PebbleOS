# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library_named(kernel)
pbl_library_kind(stlib)
pbl_library_sources(
  debug.c
  irq.c
  msgq.c
  mutex.c
  poll.c
  sched.c
  sem.c
  thread.c
  tick.c
)
pbl_library_sources_ifndef(CONFIG_ARCH_POSIX init.c)
pbl_library_include_directories(. arch)

add_subdirectory(arch)
