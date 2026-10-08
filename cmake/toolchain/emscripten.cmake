# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Emscripten, for boards that run the firmware as WebAssembly in a browser.

find_program(PBL_EM_CONFIG em-config REQUIRED)
execute_process(
  COMMAND ${PBL_EM_CONFIG} EMSCRIPTEN_ROOT
  OUTPUT_VARIABLE emscripten_root
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
include(${emscripten_root}/cmake/Modules/Platform/Emscripten.cmake)
