# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The PebbleOS package, for applications:
#
#   cmake_minimum_required(VERSION 3.20)
#   find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
#   project(hello_world)
#
#   target_sources(app PRIVATE src/main.c)
#
# It configures the build for BOARD, adds PebbleOS and defines `app`, the
# library the application's sources go to. The application's prj.conf and
# boards/<board>.conf are applied on top of the board's configuration, and
# its Kconfig, when it has one, is the root of the configuration tree.

include_guard(GLOBAL)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH PBL_BASE)
set(PBL_APPLICATION_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH ${PBL_BASE}/cmake/modules)

include(os)

add_library(app OBJECT)
target_link_libraries(app PRIVATE pbl_interface)
add_dependencies(app pbl_generated_headers)
set_property(GLOBAL APPEND PROPERTY PBL_OBJECT_LIBS app)
# A native build's main() is the host process entry.
if(CONFIG_ARCH_POSIX)
  target_compile_definitions(app PRIVATE main=pbl_app_main)
endif()
