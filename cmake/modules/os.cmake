# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# PebbleOS itself: configures the build for a board and adds the kernel, the
# SoC, the drivers, the subsystems and the libraries, for the application
# cmake/PebbleOSConfig.cmake was found from, the firmware included.
#
# Included before the first project() call, with PBL_BASE and
# PBL_APPLICATION_SOURCE_DIR set. The image is linked once the
# application's CMakeLists.txt has been read to its end.

set(PBL_APPLICATION_SOURCE_DIR ${PBL_APPLICATION_SOURCE_DIR} CACHE INTERNAL
  "Source directory of the application")

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

# Boards that do not run on ARM bring their own toolchain, and so can
# their variants (BOARD[@REVISION][/VARIANT]).
string(REGEX REPLACE "[@/].*$" "" board_name "${BOARD}")
string(REGEX MATCH "/.+$" board_variant "${BOARD}")
string(REPLACE "/" "" board_variant "${board_variant}")
if(NOT CMAKE_TOOLCHAIN_FILE)
  if(board_variant AND EXISTS ${PBL_BASE}/boards/${board_name}/toolchain.${board_variant}.cmake)
    set(CMAKE_TOOLCHAIN_FILE ${PBL_BASE}/boards/${board_name}/toolchain.${board_variant}.cmake)
  elseif(EXISTS ${PBL_BASE}/boards/${board_name}/toolchain.cmake)
    set(CMAKE_TOOLCHAIN_FILE ${PBL_BASE}/boards/${board_name}/toolchain.cmake)
  else()
    set(CMAKE_TOOLCHAIN_FILE ${PBL_BASE}/cmake/toolchain/arm-none-eabi.cmake)
  endif()
endif()

# ccache, when it is around.
find_program(CCACHE ccache)
if(CCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
  set(CMAKE_ASM_COMPILER_LAUNCHER ${CCACHE})
endif()

project(pebbleos VERSION 1.0 LANGUAGES C ASM)

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

# Build steps that call the toolchain by name -- the native SDK generator
# asks for arm-none-eabi-gcc -- find it next to the compiler CMake chose, or
# in the SDK when that compiler is another one.
get_filename_component(toolchain_bin ${CMAKE_C_COMPILER} DIRECTORY)
if(CONFIG_REQUIRES_ARM_TOOLCHAIN)
  find_program(PBL_ARM_GCC arm-none-eabi-gcc HINTS ${PEBBLEOS_SDK_HINTS})
  if(NOT PBL_ARM_GCC)
    message(FATAL_ERROR "The build needs the ARM toolchain of the PebbleOS SDK "
                        "(CONFIG_REQUIRES_ARM_TOOLCHAIN): install the SDK")
  endif()
  get_filename_component(arm_bin ${PBL_ARM_GCC} DIRECTORY)
  set(toolchain_bin ${toolchain_bin}:${arm_bin})
endif()
set(PBL_TOOLCHAIN_ENV ${CMAKE_COMMAND} -E env "PATH=${toolchain_bin}:$ENV{PATH}")
include(extensions)
include(platform)
include(compiler)
include(libc)
include(linker)
include(firmware)

message(STATUS "Board: ${PBL_BOARD} (${PBL_PLATFORM_NAME}), variant: ${VARIANT}")

pbl_init()
if(PBL_LIBC_DEFINITIONS)
  pbl_compile_definitions(${PBL_LIBC_DEFINITIONS})
endif()

# The generated headers every firmware library may include.
add_custom_target(pbl_generated_headers)

# FIXME: remove include/pbl once all modules use the prefix
pbl_include_directories(${PBL_BASE}/include ${PBL_BASE}/include/pbl)
pbl_include_directories(${CMAKE_BINARY_DIR}/generated/include)

# The firmware reports the version it was built from.
set(version_h ${CMAKE_BINARY_DIR}/generated/include/pbl/version.h)
add_custom_command(
  OUTPUT ${version_h} ${CMAKE_BINARY_DIR}/generated/version.stamp
  COMMAND ${PBL_TOOLCHAIN_ENV} ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/generate.py version
          --template ${PBL_BASE}/include/pbl/version.h.in
          --output ${version_h}
  DEPENDS ${PBL_BASE}/include/pbl/version.h.in
  WORKING_DIRECTORY ${PBL_BASE}
  COMMENT "Checking the firmware version"
  VERBATIM
)
# The stamp is never written, so the version is re-checked on every build;
# the header itself is only rewritten when it actually changes.
add_custom_target(pbl_version DEPENDS ${version_h})
add_dependencies(pbl_generated_headers pbl_version)

# An application outside this tree has its own build directories, so every
# directory here is given the one it has in a firmware build.
foreach(dir third_party lib kernel soc subsys drivers boards)
  add_subdirectory(${PBL_BASE}/${dir} ${CMAKE_BINARY_DIR}/${dir})
endforeach()

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL pbl_link_firmware)
