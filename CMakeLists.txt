# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# PebbleOS firmware build.
#
#   cmake -B build -GNinja -DBOARD=asterix
#   cmake --build build
#
# Pass Kconfig overrides the same way: -DCONFIG_RELEASE=y. See
# docs/development/build_system.md.

cmake_minimum_required(VERSION 3.20)

set(PBL_BASE ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${PBL_BASE}/cmake/modules)

set(BOARD "" CACHE STRING "Board to build for, e.g. asterix or obelix@pvt")
set(VARIANT "normal" CACHE STRING "Firmware variant: normal or prf")
set_property(CACHE VARIANT PROPERTY STRINGS normal prf)

if(NOT BOARD)
  message(FATAL_ERROR
    "No board selected. Configure with -DBOARD=<board>; the available "
    "boards are the directories under boards/.")
endif()
if(NOT VARIANT MATCHES "^(normal|prf)$")
  message(FATAL_ERROR "VARIANT must be 'normal' or 'prf', got '${VARIANT}'")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

# The toolchain and the device tools come from the installed SDK.
include(pebbleos_sdk)

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE ${PBL_BASE}/cmake/toolchain/arm-none-eabi.cmake)
endif()

# ccache, when it is around.
find_program(CCACHE ccache)
if(CCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
  set(CMAKE_ASM_COMPILER_LAUNCHER ${CCACHE})
endif()

project(pebbleos VERSION 1.0 LANGUAGES C ASM)

# Build steps that call the toolchain by name -- the native SDK generator
# asks for arm-none-eabi-gcc -- find it next to the compiler CMake chose.
get_filename_component(toolchain_bin ${CMAKE_C_COMPILER} DIRECTORY)
set(PBL_TOOLCHAIN_ENV ${CMAKE_COMMAND} -E env "PATH=${toolchain_bin}:$ENV{PATH}")

# Kconfig symbols given on the command line (-DCONFIG_FOO=y) are passed
# through to Kconfig as overrides.
set(CONFIG_OVERRIDES "")
get_cmake_property(cache_variables CACHE_VARIABLES)
foreach(var ${cache_variables})
  if(var MATCHES "^CONFIG_")
    list(APPEND CONFIG_OVERRIDES "${var}=${${var}}")
  endif()
endforeach()

include(kconfig)
include(extensions)
include(platform)
include(compiler)
include(libc)
include(linker)
include(gettext)
include(resources)
include(native_sdk)
include(sdk)
include(firmware)

message(STATUS "Board: ${PBL_BOARD} (${PBL_PLATFORM_NAME}), variant: ${VARIANT}")

pbl_init()

# The generated headers every firmware library may include.
add_custom_target(pbl_generated_headers)

# FIXME: remove include/pbl once all modules use the prefix
pbl_include_directories(include include/pbl)

# The recovery firmware has no third-party apps, so it needs neither the
# SDK shims nor the stored apps.
if(NOT CONFIG_RECOVERY_FW)
  pbl_native_sdk()
  pbl_sdk()
  add_dependencies(pbl_generated_headers pbl_native_sdk)
  set(PBL_FIRMWARE_SOURCES ${PBL_PEBBLE_AUTO_C})
endif()

add_subdirectory(third_party)
add_subdirectory(lib)
add_subdirectory(kernel)
add_subdirectory(soc)
add_subdirectory(subsys)
add_subdirectory(src)
if(NOT CONFIG_RECOVERY_FW)
  add_subdirectory(apps/stored)
endif()

pbl_resources()
pbl_link_firmware()
