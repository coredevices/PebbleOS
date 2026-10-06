# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  uzlib/src/tinflate.c
  uzlib/src/adler32.c
  uzlib/src/crc32.c
)
set(includes uzlib/src)
pbl_include_directories(${includes})
pbl_compile_definitions(UZLIB_CONF_PARANOID_CHECKS=1 UZLIB_CONF_LENGTHS_IN_STATE=1)
pbl_library_named(uzlib)
pbl_library_kind(stlib)
pbl_library_sources(${sources})
pbl_library_compile_options(-Wno-sign-compare)
