# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The Pebble watch firmware, an application of PebbleOS.
#
#   cmake -B build -GNinja -S pebble -DBOARD=asterix
#   cmake --build build
#
# Pass Kconfig overrides the same way: -DCONFIG_RELEASE=y. See
# docs/development/build_system/index.md.

cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS ${CMAKE_CURRENT_SOURCE_DIR}/..)
project(pebble LANGUAGES C ASM)

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
if(CONFIG_SAMSA)
  add_subdirectory(${PBL_BASE}/subsys/samsa ${CMAKE_BINARY_DIR}/subsys/samsa)
endif()

# The firmware's directories build under pebble/, where the generators and
# the SDK tools expect what they generate.
set(pebble_bin ${CMAKE_BINARY_DIR}/pebble)
file(MAKE_DIRECTORY ${pebble_bin})
pbl_include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${pebble_bin})

set(dirs
  shell
  services
  applib
  mfg
  comm
  debug
  kernel
  popups
  process_management
  process_state
  resource
  syscall
  system
  apps/core
)
if(CONFIG_RECOVERY_FW)
  list(APPEND dirs apps/prf)
else()
  list(APPEND dirs apps)
endif()
foreach(dir ${dirs})
  add_subdirectory(${dir} ${pebble_bin}/${dir})
endforeach()

file(GLOB sources CONFIGURE_DEPENDS *.c)
if(CONFIG_ARCH_POSIX)
  pbl_filter_sources(sources EXCLUDE hardfault_handler.c)
endif()
target_sources(app PRIVATE ${sources})

if(NOT CONFIG_RECOVERY_FW)
  # One universal catalog, independent of board and enabled services.
  file(GLOB_RECURSE pot_sources CONFIGURE_DEPENDS *.c *.h *.def)
  pbl_firmware_pot(${PROJECT_BINARY_DIR}/pebbleos.pot ${pot_sources}
    EXCLUDE apps/prf/**)
endif()

# The stored apps are built for the watch, which the recovery firmware runs
# none on.
if(NOT CONFIG_RECOVERY_FW AND NOT CONFIG_SOC_POSIX)
  add_subdirectory(${PBL_BASE}/apps/stored ${CMAKE_BINARY_DIR}/apps/stored)
endif()

pbl_resources()
