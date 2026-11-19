# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_RECOVERY_FW)
  set(source bluetooth_pairing_ui.c)
else()
  file(GLOB_RECURSE source CONFIGURE_DEPENDS *.c)
endif()
pbl_library()
pbl_library_sources(${source})
