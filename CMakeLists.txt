# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# PebbleOS firmware build.
#
#   cmake -B build -GNinja -DBOARD=asterix
#   cmake --build build
#
# Pass Kconfig overrides the same way: -DCONFIG_RELEASE=y. See
# docs/development/build_system/index.md.

cmake_minimum_required(VERSION 3.20)

set(PBL_BASE ${CMAKE_CURRENT_SOURCE_DIR})
set(PBL_APPLICATION_SOURCE_DIR ${PBL_BASE}/pebble)
list(APPEND CMAKE_MODULE_PATH ${PBL_BASE}/cmake/modules)

include(os)

project(pebbleos VERSION 1.0 LANGUAGES C ASM)

include(gettext)
include(nanopb)
include(resources)
include(native_sdk)
include(sdk)

# Third-party apps call the firmware through the SDK shims. The recovery
# firmware runs none, and a firmware for a CPU other than a watch's only runs
# them with Samsa interpreting them. The shippable SDK is for the watch's.
if(NOT CONFIG_RECOVERY_FW AND (CONFIG_CPU_CORTEX_M OR CONFIG_SAMSA))
  pbl_native_sdk()
  add_dependencies(pbl_generated_headers pbl_native_sdk)
  set(PBL_FIRMWARE_SOURCES ${PBL_PEBBLE_AUTO_C})
endif()
if(NOT CONFIG_RECOVERY_FW AND CONFIG_CPU_CORTEX_M)
  pbl_sdk()
endif()

# Samsa runs the apps built against the SDK generated above.
pbl_add_subdirectory_ifdef(CONFIG_SAMSA subsys/samsa)
add_subdirectory(pebble)
# The stored apps are built for the watch, which the recovery firmware runs
# none on.
if(NOT CONFIG_RECOVERY_FW AND NOT CONFIG_SOC_POSIX)
  add_subdirectory(apps/stored)
endif()

pbl_resources()
