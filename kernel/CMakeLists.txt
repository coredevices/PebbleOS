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
  mpu.c
  mpu_region.c
  msgq.c
  mutex.c
  poll.c
  print.c
  reboot.c
  sched.c
  scratch.c
  sem.c
  syscall.c
  thread.c
  tick.c
  time.c
  timekeeping.c
  timer.c
  wallclock.c
  sys_work.c
  work.c
)
pbl_library_sources_ifdef(CONFIG_CPU_IDLE idle.c)
if(CONFIG_CPU_IDLE)
  pbl_iterable_section(idle_hooks)
  pbl_iterable_section(idle_late RAMFUNC)
endif()
# The heap poisons what it hands out but walks its own memory freely.
if(CONFIG_ASAN)
  set_source_files_properties(heap.c PROPERTIES COMPILE_OPTIONS -fno-sanitize=address)
endif()
pbl_library_include_directories(. arch)
pbl_iterable_section(sys_off)

add_subdirectory(arch)
