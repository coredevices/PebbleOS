# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.21)
project(pebble-text2wasm LANGUAGES C)

if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Configure with emcmake cmake to select the Emscripten toolchain")
endif()
find_package(Git REQUIRED)
get_filename_component(PEBBLEOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(DIST "${CMAKE_CURRENT_BINARY_DIR}/dist")
set(OVERRIDES "${CMAKE_CURRENT_BINARY_DIR}/overrides")
file(MAKE_DIRECTORY "${DIST}" "${OVERRIDES}/kernel" "${OVERRIDES}/resource")

# The browser does not use events; its time ABI differs from the watch's.
set(EVENTS_HEADER "${PEBBLEOS_ROOT}/src/fw/kernel/events.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EVENTS_HEADER}")
file(READ "${EVENTS_HEADER}" events)
string(REPLACE "#if __SIZEOF_POINTER__ == 4"
  "#if __SIZEOF_POINTER__ == 4 && !defined(__EMSCRIPTEN__)" events "${events}")
file(CONFIGURE OUTPUT "${OVERRIDES}/kernel/events.h" CONTENT "${events}" @ONLY)
file(CONFIGURE OUTPUT "${OVERRIDES}/resource/resource_ids.auto.h"
  CONTENT "#pragma once\n#define RESOURCE_ID_FONT_FALLBACK_INTERNAL 0xffffffff\n" @ONLY)

add_executable(pebble_renderer
  renderer.c
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/8_bit/framebuffer.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/framebuffer.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/gcolor_definitions.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/gtypes.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/gbitmap.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/graphics.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/gtransform.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/bitblt.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/8_bit/bitblt_private.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/text_render.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/utf8.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/text_layout.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/rtl_support.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/arabic_shaping.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/perimeter.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/text_resources.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/graphics_private.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/graphics_private_raw.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/graphics_circle.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/graphics/graphics_line.c"
  "${PEBBLEOS_ROOT}/src/fw/applib/fonts/codepoint.c"
  "${PEBBLEOS_ROOT}/lib/util/keyed_circular_cache.c"
  "${PEBBLEOS_ROOT}/lib/util/math.c"
  "${PEBBLEOS_ROOT}/lib/util/trig.c"
  "${PEBBLEOS_ROOT}/lib/util/iterator.c"
)
set_target_properties(pebble_renderer PROPERTIES
  OUTPUT_NAME renderer
  SUFFIX .js
  RUNTIME_OUTPUT_DIRECTORY "${DIST}"
)
target_include_directories(pebble_renderer PRIVATE
  "${OVERRIDES}"
  "${PEBBLEOS_ROOT}/src/fw/util/time"
  "${PEBBLEOS_ROOT}/tests/overrides/dummy_board"
  "${PEBBLEOS_ROOT}/tests/overrides/default"
  "${PEBBLEOS_ROOT}/tests/stubs"
  "${PEBBLEOS_ROOT}/tests/fakes"
  "${PEBBLEOS_ROOT}/tests/test_includes"
  "${PEBBLEOS_ROOT}/tests"
  "${PEBBLEOS_ROOT}/include"
  "${PEBBLEOS_ROOT}/include/pbl"
  "${PEBBLEOS_ROOT}/kernel/arch/posix/include"
  "${PEBBLEOS_ROOT}/subsys"
  "${PEBBLEOS_ROOT}/src/fw"
  "${PEBBLEOS_ROOT}/src/fw/applib/vendor/uPNG"
  "${PEBBLEOS_ROOT}/src/fw/applib/vendor/tinflate"
  "${PEBBLEOS_ROOT}/tests/fixtures"
)
target_compile_definitions(pebble_renderer PRIVATE
  UNITTEST
  CONFIG_SCREEN_COLOR_DEPTH_BITS=8
  DISP_COLS=144
  DISP_ROWS=168
  DISPLAY_FRAMEBUFFER_BYTES=24192
  PBL_COLOR
  PBL_RECT
  CONFIG_BOARD_OBELIX=1
  CONFIG_PLATFORM_EMERY=1
  CONFIG_FLASH_GD25Q256E=1
  CONFIG_KERNEL_NUM_PRIORITIES=5
  CONFIG_KERNEL_THREAD_TLS_SLOTS=2
  CONFIG_KERNEL_STACK_ALIGN=8
  CONFIG_KERNEL_TICK_HZ=1024
)
target_compile_options(pebble_renderer PRIVATE
  -O2
  -Wno-error=incompatible-pointer-types
  -Wno-gnu-variable-sized-type-not-at-end
)
target_link_options(pebble_renderer PRIVATE
  -O2
  -Wl,--error-limit=0
  -sSINGLE_FILE=1
  -sMODULARIZE=1
  -sEXPORT_NAME=createPebbleRenderer
  -sALLOW_MEMORY_GROWTH=1
  "-sEXPORTED_FUNCTIONS=['_render','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['ccall','HEAPU8']"
)

set(font_outputs)
foreach(directory normal common)
  file(GLOB fonts CONFIGURE_DEPENDS "${PEBBLEOS_ROOT}/resources/${directory}/base/pbf/GOTHIC*.pbf")
  foreach(font IN LISTS fonts)
    get_filename_component(name "${font}" NAME)
    set(output "${DIST}/${name}")
    add_custom_command(OUTPUT "${output}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${font}" "${output}"
      DEPENDS "${font}"
      VERBATIM)
    list(APPEND font_outputs "${output}")
  endforeach()
endforeach()

add_custom_target(text2wasm ALL
  COMMAND ${CMAKE_COMMAND}
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DPEBBLEOS_ROOT=${PEBBLEOS_ROOT}"
    "-DOUTPUT=${DIST}/revision.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/WriteRevision.cmake"
  DEPENDS pebble_renderer ${font_outputs}
  BYPRODUCTS "${DIST}/revision.txt"
  VERBATIM)
