# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(endpoints ${CMAKE_CURRENT_BINARY_DIR}/endpoints.auto.c)
if(CONFIG_RECOVERY_FW)
  set(recovery --recovery)
else()
  set(recovery "")
endif()
add_custom_command(
  OUTPUT ${endpoints}
  COMMAND ${PBL_TOOLCHAIN_ENV} ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/generate.py endpoints-table
          --input ${CMAKE_CURRENT_SOURCE_DIR}/endpoints.json
          --output ${endpoints} ${recovery}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/endpoints.json ${PBL_BASE}/tools/cmake/generate.py
  WORKING_DIRECTORY ${PBL_BASE}
  COMMENT "Generating the Pebble protocol endpoint table"
  VERBATIM
)

pbl_library()
pbl_library_sources(
  analytics.c
  kernel_receiver.c
  meta.c
  receive.c
  send_buffer.c
  send_queue.c
  session.c
  version.c
  ${endpoints}
)
pbl_linker_sources(rom-start pp.ld)
