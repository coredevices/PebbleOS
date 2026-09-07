# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Bare-metal ARM toolchain. Architecture flags are not set here: they are
# derived from Kconfig by cmake/modules/compiler.cmake once the
# configuration is known.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED CROSS_COMPILE)
  set(CROSS_COMPILE arm-none-eabi-)
endif()

# The installed PebbleOS SDK is searched ahead of PATH.
include(${CMAKE_CURRENT_LIST_DIR}/../modules/pebbleos_sdk.cmake)

find_program(CMAKE_C_COMPILER ${CROSS_COMPILE}gcc HINTS ${PEBBLEOS_SDK_HINTS} REQUIRED)
find_program(CMAKE_ASM_COMPILER ${CROSS_COMPILE}gcc HINTS ${PEBBLEOS_SDK_HINTS} REQUIRED)
find_program(CMAKE_AR ${CROSS_COMPILE}gcc-ar HINTS ${PEBBLEOS_SDK_HINTS} REQUIRED)
find_program(CMAKE_RANLIB ${CROSS_COMPILE}gcc-ranlib HINTS ${PEBBLEOS_SDK_HINTS} REQUIRED)
find_program(CMAKE_OBJCOPY ${CROSS_COMPILE}objcopy HINTS ${PEBBLEOS_SDK_HINTS} REQUIRED)
find_program(CMAKE_OBJDUMP ${CROSS_COMPILE}objdump HINTS ${PEBBLEOS_SDK_HINTS} REQUIRED)
find_program(CMAKE_SIZE ${CROSS_COMPILE}size HINTS ${PEBBLEOS_SDK_HINTS})
find_program(CMAKE_NM ${CROSS_COMPILE}nm HINTS ${PEBBLEOS_SDK_HINTS})

# The firmware supplies its own startup code and links against a linker
# script that is only generated later, so a full link test cannot succeed.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Deterministic archives: no timestamps or UIDs inside .a files.
set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcsD <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> qsD <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
