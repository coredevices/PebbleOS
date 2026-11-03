# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library_named(kernel)
pbl_library_kind(stlib)
pbl_library_sources(
  debug.c
  fatal.c
  heap.c
  init.c
  irq.c
  malloc.c
  msgq.c
  mutex.c
  poll.c
  reboot.c
  sched.c
  sem.c
  syscall.c
  thread.c
  tick.c
  time.c
  timekeeping.c
  timer.c
)
# The heap poisons what it hands out but walks its own memory freely.
if(CONFIG_ASAN)
  set_source_files_properties(heap.c PROPERTIES COMPILE_OPTIONS -fno-sanitize=address)
endif()
pbl_library_include_directories(. arch)
pbl_linker_sources(rom-start reboot.ld)

add_subdirectory(arch)
