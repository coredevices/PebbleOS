# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

file(GLOB_RECURSE sources CONFIGURE_DEPENDS *.c)

pbl_library_named(libutil)
pbl_library_kind(stlib)
pbl_library_sources(${sources})
# The heap poisons what it hands out but walks its own memory freely.
if(CONFIG_ASAN)
  set_source_files_properties(heap.c PROPERTIES COMPILE_OPTIONS -fno-sanitize=address)
endif()
pbl_library_include_directories(.)
