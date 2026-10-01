# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

find_program(NANOPB_GENERATOR nanopb_generator REQUIRED)

# Compile protobuf schemas into the current library. A <name>.options file
# next to a schema tunes its generated C types.
function(pbl_library_nanopb_sources)
  set(protos "")
  foreach(proto ${ARGN})
    get_filename_component(proto ${proto} ABSOLUTE)
    list(APPEND protos ${proto})
  endforeach()

  set(sources "")
  set(headers "")
  foreach(proto ${protos})
    get_filename_component(name ${proto} NAME_WE)
    get_filename_component(dir ${proto} DIRECTORY)
    set(generated_c ${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.c)
    set(generated_h ${CMAKE_CURRENT_BINARY_DIR}/${name}.pb.h)
    file(GLOB options CONFIGURE_DEPENDS ${dir}/${name}.options)
    add_custom_command(
      OUTPUT ${generated_c} ${generated_h}
      COMMAND ${NANOPB_GENERATOR} -q -I ${dir}
              -D ${CMAKE_CURRENT_BINARY_DIR} ${proto}
      DEPENDS ${protos} ${options}
      COMMENT "Generating ${name}.pb.c"
      VERBATIM
    )
    list(APPEND sources ${generated_c})
    list(APPEND headers ${generated_h})
  endforeach()

  add_custom_target(${PBL_CURRENT_LIBRARY}__nanopb DEPENDS ${headers})
  add_dependencies(pbl_generated_headers ${PBL_CURRENT_LIBRARY}__nanopb)

  pbl_library_sources(${sources})
  pbl_library_include_directories(${CMAKE_CURRENT_BINARY_DIR})
endfunction()
