# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Linker script assembly. Libraries that need additions to the master
# script (fw/linker/pebbleos.ld) register fragments against one of its
# hook points; the fragments are aggregated into snippets-<location>.ld
# files that the master script includes.

# Whether the image is linked with the firmware linker script. A native build
# links with the host's default one instead: Mach-O linkers cannot gather
# sections sorted by name, and ELF hosts do not insert the fragments (yet).
# Without the script, PBL_SECTION() is a no-op and code collecting objects
# across files uses PBL_UNSORTED_SECTION().
if(CONFIG_ARCH_POSIX)
  set(PBL_LINKER_SCRIPT OFF)
  add_compile_definitions(PBL_NO_LINKER_SCRIPT)
else()
  set(PBL_LINKER_SCRIPT ON)
endif()

set(PBL_LINKER_SNIPPET_LOCATIONS memory rom-start ram-sections ramfunc footer)

foreach(location ${PBL_LINKER_SNIPPET_LOCATIONS})
  define_property(GLOBAL PROPERTY PBL_LINKER_SNIPPETS_${location}
    BRIEF_DOCS "Linker script fragments for the ${location} hook point")
endforeach()

define_property(GLOBAL PROPERTY PBL_RAMFUNC_OBJECTS
  BRIEF_DOCS "Linker input patterns for the objects placed in .ramfunc")

# Fragments are included sorted by (sort key, registration order).
function(pbl_linker_sources location)
  cmake_parse_arguments(ARG "" "SORT_KEY" "" ${ARGN})
  if(NOT ARG_SORT_KEY)
    set(ARG_SORT_KEY "default")
  endif()
  if(NOT location IN_LIST PBL_LINKER_SNIPPET_LOCATIONS)
    message(FATAL_ERROR
      "pbl_linker_sources(): unknown location '${location}' (must be one of "
      "${PBL_LINKER_SNIPPET_LOCATIONS})")
  endif()
  foreach(src ${ARG_UNPARSED_ARGUMENTS})
    if(NOT IS_ABSOLUTE ${src})
      set(src ${CMAKE_CURRENT_SOURCE_DIR}/${src})
    endif()
    if(NOT EXISTS ${src})
      message(FATAL_ERROR "pbl_linker_sources(): ${src} not found")
    endif()
    set_property(GLOBAL APPEND PROPERTY PBL_LINKER_SNIPPETS_${location}
      "${ARG_SORT_KEY}|${src}")
  endforeach()
endfunction()

# Run the given sources of the current library from RAM: their code and
# read-only data go to .ramfunc. A no-op without CONFIG_RAMFUNC.
function(pbl_library_ramfunc)
  if(NOT CONFIG_RAMFUNC)
    return()
  endif()
  pbl_library_ensure()
  foreach(source ${ARGN})
    if(IS_ABSOLUTE ${source})
      file(RELATIVE_PATH source ${CMAKE_CURRENT_SOURCE_DIR} ${source})
    endif()
    string(REPLACE "../" "__/" object ${source}${CMAKE_C_OUTPUT_EXTENSION})
    if(PBL_CURRENT_LIBRARY_KIND STREQUAL "stlib")
      get_filename_component(object ${object} NAME)
      set(archive ${CMAKE_STATIC_LIBRARY_PREFIX}${PBL_CURRENT_LIBRARY}${CMAKE_STATIC_LIBRARY_SUFFIX})
      set(pattern "*${archive}:${object}")
    else()
      set(pattern "*/CMakeFiles/${PBL_CURRENT_LIBRARY}.dir/${object}")
    endif()
    set_property(GLOBAL APPEND PROPERTY PBL_RAMFUNC_OBJECTS "${pattern}")
  endforeach()
endfunction()

# Writes the snippet files and returns the generated linker script the
# firmware links against, plus everything it depends on.
function(pbl_linker_script out_script out_depends)
  set(dir ${PROJECT_BINARY_DIR}/linker)
  file(MAKE_DIRECTORY ${dir})
  set(depends "")

  foreach(location ${PBL_LINKER_SNIPPET_LOCATIONS})
    get_property(entries GLOBAL PROPERTY PBL_LINKER_SNIPPETS_${location})
    list(SORT entries)
    set(content "")
    foreach(entry ${entries})
      string(REGEX REPLACE "^[^|]*\\|" "" fragment ${entry})
      string(APPEND content "#include \"${fragment}\"\n")
      list(APPEND depends ${fragment})
    endforeach()
    set(snippet ${dir}/snippets-${location}.ld)
    file(CONFIGURE OUTPUT ${snippet} CONTENT "${content}" @ONLY)
    list(APPEND depends ${snippet})
  endforeach()

  get_property(objects GLOBAL PROPERTY PBL_RAMFUNC_OBJECTS)
  set(content "")
  foreach(object ${objects})
    string(APPEND content "${object} (.text* .rodata*)\n")
  endforeach()
  file(CONFIGURE OUTPUT ${dir}/ramfunc-objects.ld CONTENT "${content}" @ONLY)
  list(APPEND depends ${dir}/ramfunc-objects.ld)

  # Fragments included by the master script and by the SoC fragments.
  file(GLOB_RECURSE common CONFIGURE_DEPENDS ${PBL_BASE}/fw/linker/*.ld)
  list(APPEND depends ${common} ${PBL_AUTOCONF_H})

  set(master ${PBL_BASE}/fw/linker/pebbleos.ld)
  set(script ${PROJECT_BINARY_DIR}/pebbleos.ld.pre)

  # The linker script goes through the C preprocessor, which gives it
  # access to the Kconfig symbols, #include and #if.
  add_custom_command(
    OUTPUT ${script}
    COMMAND ${CMAKE_C_COMPILER} -x assembler-with-cpp -nostdinc -undef -E -P
            -I${dir} -I${PBL_BASE}/fw/linker
            -include ${PBL_AUTOCONF_H} ${master} -o ${script}
    DEPENDS ${master} ${depends}
    COMMENT "Preprocessing linker script"
    VERBATIM
  )

  set(${out_script} ${script} PARENT_SCOPE)
  set(${out_depends} "${depends}" PARENT_SCOPE)
endfunction()
