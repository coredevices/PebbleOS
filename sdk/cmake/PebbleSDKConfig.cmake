# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The Pebble SDK, for apps, watchfaces and Pebble packages built with
# CMake. A project's CMakeLists.txt finds it before project(), which lets
# it pick the toolchain:
#
#   cmake_minimum_required(VERSION 3.20)
#   find_package(PebbleSDK REQUIRED HINTS $ENV{PEBBLE_SDK})
#   project(my-app C)
#   pebble_app(SOURCES src/c/main.c)
#
# package.json describes the project; finding the SDK reads it. See
# pebble.cmake for the functions a project calls.

include_guard(GLOBAL)

if(DEFINED PROJECT_NAME)
  message(FATAL_ERROR
    "find_package(PebbleSDK) has to come before project(): it selects the "
    "toolchain the project is built with")
endif()

get_filename_component(PEBBLE_SDK_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
# An SDK installed by pebble-tool keeps its Python environment, node
# modules and toolchain two levels above.
get_filename_component(_pebble_install ${PEBBLE_SDK_ROOT}/../.. ABSOLUTE)

if(NOT CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug or Release" FORCE)
endif()
# Set ahead of the compiler's defaults (-O3 -DNDEBUG): apps are built for
# size.
set(CMAKE_C_FLAGS_RELEASE "-Os" CACHE STRING "")
set(CMAKE_C_FLAGS_DEBUG "-O0" CACHE STRING "")
# The language apps have always been built as; a project may pick another.
if(NOT DEFINED CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
  set(CMAKE_C_EXTENSIONS OFF)
endif()

find_program(PEBBLE_PYTHON NAMES python3 python
  HINTS ${_pebble_install}/.venv/bin NO_DEFAULT_PATH)
find_program(PEBBLE_PYTHON NAMES python3 python REQUIRED)
set(PEBBLE_NODE_MODULES ${_pebble_install}/node_modules CACHE PATH
  "node_modules holding webpack, for projects with enableMultiJS")

set(PEBBLE_PLATFORMS "" CACHE STRING
  "Build for these of the project's targetPlatforms only (all when empty)")
set(PEBBLE_TIMESTAMP "" CACHE STRING
  "Timestamp stamped into the binaries (the configure time when empty)")
option(PEBBLE_WARNINGS_AS_ERRORS "Fail the build on compiler warnings" OFF)

include(${CMAKE_CURRENT_LIST_DIR}/pebble.cmake)
_pebble_configure()

set(PebbleSDK_FOUND TRUE)
