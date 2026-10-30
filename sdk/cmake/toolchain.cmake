# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The ARM toolchain apps are built with: the one installed next to the SDK
# first, then whatever PATH has.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

get_filename_component(_pebble_toolchain
  ${CMAKE_CURRENT_LIST_DIR}/../../../toolchain/arm-none-eabi/bin ABSOLUTE)

foreach(tool gcc gcc-ar gcc-ranlib objcopy size)
  string(TOUPPER ${tool} var)
  string(REPLACE "-" "_" var ${var})
  find_program(PEBBLE_${var} arm-none-eabi-${tool} HINTS ${_pebble_toolchain} REQUIRED)
endforeach()

set(CMAKE_C_COMPILER ${PEBBLE_GCC})
set(CMAKE_AR ${PEBBLE_GCC_AR})
set(CMAKE_RANLIB ${PEBBLE_GCC_RANLIB})
set(CMAKE_OBJCOPY ${PEBBLE_OBJCOPY})
set(CMAKE_SIZE ${PEBBLE_SIZE})

# Deterministic archives: no timestamps or UIDs inside packages' .a files.
set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcsD <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> qsD <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>")

# Apps link against a linker script the build generates.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
