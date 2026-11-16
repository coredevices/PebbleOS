# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

find_package(PebbleOS REQUIRED HINTS $ENV{PEBBLEOS_BASE})
project(i18n_catalog)

# The catalog is compiled from its .po and embedded into the image with #embed.
find_program(MSGFMT msgfmt REQUIRED)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/fr_FR.mo
  COMMAND ${MSGFMT} --check -o ${CMAKE_CURRENT_BINARY_DIR}/fr_FR.mo
          ${CMAKE_CURRENT_SOURCE_DIR}/po/fr_FR.po
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/po/fr_FR.po
)
add_custom_target(i18n_catalog_mo DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/fr_FR.mo)
add_dependencies(app i18n_catalog_mo)

target_sources(app PRIVATE src/main.c)
target_compile_options(app PRIVATE --embed-dir=${CMAKE_CURRENT_BINARY_DIR})
