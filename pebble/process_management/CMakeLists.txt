# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(excl)
if(CONFIG_RECOVERY_FW)
  list(APPEND excl
    app_custom_icon.c
    app_menu_data_source.c
  )
endif()
if(PBL_LINKER_SCRIPT)
  pbl_linker_sources(memory ram.ld)
  list(APPEND excl ram_native.c)
endif()
pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_filter_sources(sources EXCLUDE ${excl})
pbl_library_sources(${sources})
