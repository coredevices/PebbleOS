# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  assert.c
  libutil.c
  syscalls.c
)
pbl_library_sources_ifdef(CONFIG_LIBC_NEWLIB lock.c)
pbl_library_sources_ifdef(CONFIG_LIBC_NEWLIB_NANO lock.c)
pbl_library_sources_ifdef(CONFIG_LIBC_PICOLIBC lock.c)
if(CONFIG_LIBC_NEWLIB OR CONFIG_LIBC_NEWLIB_NANO)
  add_subdirectory(newlib)
endif()
