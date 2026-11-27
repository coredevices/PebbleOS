# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# Generate a CANopen object dictionary from a YAML file:
#
#   pbl_canopen_od(od.yaml [NAME <symbol>] [TARGET <target>])
#
# Adds <symbol>.c to the target (app by default) and puts <symbol>.h, which
# declares the dictionary as `const struct pbl_canopen_od <symbol>`, on its
# include path. The symbol defaults to the file name without its extension.
# <symbol>.eds, for the tools on the other end of the bus, lands next to them.
function(pbl_canopen_od yaml)
  cmake_parse_arguments(ARG "" "NAME;TARGET" "" ${ARGN})
  if(ARG_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "pbl_canopen_od(): unknown arguments ${ARG_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT ARG_TARGET)
    set(ARG_TARGET app)
  endif()
  get_filename_component(yaml ${yaml} ABSOLUTE)
  if(NOT ARG_NAME)
    get_filename_component(ARG_NAME ${yaml} NAME_WE)
  endif()

  set(tool ${PBL_BASE}/tools/cmake/canopen_od.py)
  set(dir ${CMAKE_CURRENT_BINARY_DIR}/canopen)
  set(c ${dir}/${ARG_NAME}.c)
  set(h ${dir}/${ARG_NAME}.h)
  set(eds ${dir}/${ARG_NAME}.eds)
  get_filename_component(source ${yaml} NAME)

  add_custom_command(
    OUTPUT ${c} ${h} ${eds}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
    COMMAND ${PYTHON_EXECUTABLE} ${tool} ${yaml}
            --name ${ARG_NAME} --source ${source} --c ${c} --h ${h} --eds ${eds}
    DEPENDS ${yaml} ${tool}
    COMMENT "Generating the CANopen object dictionary ${ARG_NAME}"
    VERBATIM
  )

  target_sources(${ARG_TARGET} PRIVATE ${c} ${h})
  target_include_directories(${ARG_TARGET} PRIVATE ${dir})
endfunction()
