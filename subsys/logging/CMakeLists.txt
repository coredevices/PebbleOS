# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_LOG_BACKEND_UART backends/uart.c)
pbl_library_sources_ifdef(CONFIG_LOG_BACKEND_FLASH
  backends/flash.c
  backends/flash_shell.c
  backends/flash_storage.c
)
pbl_linker_sources(rom-start backends.ld)

# Hashed messages only carry the hash of their format string: the strings
# are read from the linked image into the dictionary the console and the
# bundle use to turn them back into text.
if(CONFIG_LOG_HASHED)
  set(PBL_LOG_DICT ${PROJECT_BINARY_DIR}/log.dict.json CACHE INTERNAL "Log dictionary")
  add_custom_command(
    OUTPUT ${PBL_LOG_DICT}
    COMMAND ${PBL_TOOLCHAIN_ENV} ${PYTHON_EXECUTABLE} ${PBL_GENERATE_PY} loghash
            --elf ${PROJECT_BINARY_DIR}/pebbleos.elf --output ${PBL_LOG_DICT}
    DEPENDS pebbleos ${PBL_GENERATE_PY}
    WORKING_DIRECTORY ${PBL_BASE}
    COMMENT "Checking and hashing log strings"
    VERBATIM
  )
  add_custom_target(pbl_log_dict ALL DEPENDS ${PBL_LOG_DICT})
endif()
