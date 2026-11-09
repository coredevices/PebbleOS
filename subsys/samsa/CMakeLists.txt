# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(thunks ${PROJECT_BINARY_DIR}/subsys/samsa/samsa_thunks.auto.c)
add_custom_command(
  OUTPUT ${thunks}
  COMMAND ${PBL_TOOLCHAIN_ENV} ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/samsa/generate_thunks.py
          --table ${PROJECT_BINARY_DIR}/pebble/pebble.auto.json
          --sdk-include ${PBL_SDK_DIR}/include
          --platform ${PBL_PLATFORM_NAME}
          --output ${thunks}
  DEPENDS ${PBL_BASE}/tools/samsa/generate_thunks.py
          ${PROJECT_BINARY_DIR}/pebble/pebble.auto.json
          ${PBL_SDK_DIR}/include/pebble.h
  WORKING_DIRECTORY ${PBL_BASE}
  COMMENT "Generating Samsa thunks"
  VERBATIM
)

pbl_library()
pbl_library_sources(
  samsa_manual.c
  samsa_pbw.c
  samsa_runtime.c
  samsa_types.c
  ${thunks}
)
pbl_library_include_directories(.)

pbl_host_include_directories(.)
pbl_host_sources(samsa_pbw_bottom.c)
set_property(SOURCE ${thunks} TARGET_DIRECTORY ${PBL_CURRENT_LIBRARY}
  APPEND PROPERTY INCLUDE_DIRECTORIES ${PROJECT_BINARY_DIR})
