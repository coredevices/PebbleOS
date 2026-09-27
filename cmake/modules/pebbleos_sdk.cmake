# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The installed PebbleOS SDK, which ships the toolchain, QEMU and sftool
# the firmware is meant to be built and run with.
#
# The SDK is located once and remembered in PEBBLEOS_SDK_ROOT. The tools
# are then looked up with the SDK as the first place to search, and cached
# by find_program like any other. Pass -DPEBBLEOS_SDK_ROOT=<dir> to use a
# specific install; when empty, the search order is $PEBBLEOS_SDK_ROOT
# (exported by the SDK's env.sh), then under the home directory and /opt
# the newest pebbleos-sdk-<version> satisfying SDK_VERSION, then a plain
# pebbleos-sdk directory. Without an SDK the tools come from PATH.

include_guard(GLOBAL)

set(PEBBLEOS_SDK_ROOT "" CACHE PATH
  "Installed PebbleOS SDK (located automatically when empty)")

function(_pbl_find_sdk out)
  file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../../SDK_VERSION min_version LIMIT_COUNT 1)
  string(STRIP "${min_version}" min_version)
  foreach(base "$ENV{HOME}" /opt)
    set(best "")
    set(best_version 0)
    file(GLOB dirs LIST_DIRECTORIES true "${base}/pebbleos-sdk-*")
    foreach(dir ${dirs})
      get_filename_component(name ${dir} NAME)
      if(IS_DIRECTORY ${dir} AND name MATCHES "^pebbleos-sdk-([0-9]+\\.[0-9]+\\.[0-9]+)$")
        set(version ${CMAKE_MATCH_1})
        if(version VERSION_GREATER_EQUAL min_version AND version VERSION_GREATER best_version)
          set(best ${dir})
          set(best_version ${version})
        endif()
      endif()
    endforeach()
    if(best)
      set(${out} ${best} PARENT_SCOPE)
      return()
    endif()
    if(IS_DIRECTORY ${base}/pebbleos-sdk)
      set(${out} ${base}/pebbleos-sdk PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${out} "" PARENT_SCOPE)
endfunction()

if(NOT PEBBLEOS_SDK_ROOT)
  if(DEFINED ENV{PEBBLEOS_SDK_ROOT} AND IS_DIRECTORY "$ENV{PEBBLEOS_SDK_ROOT}")
    set(root $ENV{PEBBLEOS_SDK_ROOT})
  else()
    _pbl_find_sdk(root)
  endif()
  if(root)
    set(PEBBLEOS_SDK_ROOT ${root} CACHE PATH
      "Installed PebbleOS SDK (located automatically when empty)" FORCE)
  endif()
endif()

set(PEBBLEOS_SDK_HINTS "")
if(PEBBLEOS_SDK_ROOT)
  if(NOT IS_DIRECTORY ${PEBBLEOS_SDK_ROOT})
    message(FATAL_ERROR "PEBBLEOS_SDK_ROOT is not a directory: ${PEBBLEOS_SDK_ROOT}")
  endif()
  get_filename_component(name ${PEBBLEOS_SDK_ROOT} NAME)
  if(name MATCHES "^pebbleos-sdk-([0-9.]+)$")
    set(label ${CMAKE_MATCH_1})
  else()
    set(label unversioned)
  endif()
  message(STATUS "PebbleOS SDK: ${PEBBLEOS_SDK_ROOT} (${label})")
  set(PEBBLEOS_SDK_HINTS
    ${PEBBLEOS_SDK_ROOT}/arm-none-eabi/bin
    ${PEBBLEOS_SDK_ROOT}/qemu/bin
    ${PEBBLEOS_SDK_ROOT}/sftool
  )
else()
  message(STATUS "PebbleOS SDK: none found, using the tools on PATH")
endif()

# try_compile projects re-read the toolchain file with their own cache.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PEBBLEOS_SDK_ROOT)

find_program(PBL_QEMU qemu-pebble HINTS ${PEBBLEOS_SDK_HINTS}
  DOC "Pebble QEMU, for 'pbl qemu'")
find_program(PBL_SFTOOL sftool HINTS ${PEBBLEOS_SDK_HINTS}
  DOC "SiFli flash tool, for 'pbl flash'")
find_program(PBL_GDB NAMES pebble-gdb arm-none-eabi-gdb-py arm-none-eabi-gdb
  HINTS ${PEBBLEOS_SDK_HINTS} DOC "ARM gdb, for 'pbl debug'")
