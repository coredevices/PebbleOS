# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(service.c)
pbl_library_sources_ifdef(CONFIG_SERVICE_ACCEL_MANAGER_RECORDER recorder.c)
if(CONFIG_SERVICE_ACCEL_MANAGER_RECORDER)
  pbl_library_compile_definitions(ACCEL_RECORDER_BOARD="${PBL_BOARD_NAME}")
endif()
