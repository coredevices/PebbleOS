# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Building Pebble apps, watchfaces and packages.
#
# One build directory builds every platform the project targets: package.json
# names them, PEBBLE_PLATFORMS narrows them down. The code is described once,
# on an interface target, and compiled once per platform:
#
#   pebble_app(SOURCES ...)      the app; sources and settings go on `app`,
#                                and build app_<platform> for every platform
#   pebble_worker(SOURCES ...)   the background worker, on `worker`
#   pebble_js(SOURCES ... [ENTRY file])
#                                PebbleKit JS (or a package's JS)
#   pebble_package(SOURCES ... HEADERS ...)
#                                a Pebble package, on `package`
#
# Settings for all platforms go on the interface target:
#
#   target_compile_definitions(app INTERFACE MY_OPTION=1)
#   target_include_directories(app INTERFACE src/c)
#
# and those for one platform on its target, e.g. app_emery.
#
# Apps and watchfaces build into <build>/<project directory>.pbw, packages
# into dist.zip next to package.json.

include_guard(GLOBAL)

set(_PEBBLE_TOOL ${PEBBLE_SDK_ROOT}/common/tools/pebble_build.py)

# Reads package.json, and describes the project in variables. A macro: the
# variables belong to the project's scope.
macro(_pebble_configure)
  set(_pebble_args "")
  if(PEBBLE_TIMESTAMP)
    set(_pebble_args --timestamp ${PEBBLE_TIMESTAMP})
  endif()
  execute_process(
    COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} configure
            --sdk ${PEBBLE_SDK_ROOT}
            --project-dir ${CMAKE_SOURCE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
            --platforms "${PEBBLE_PLATFORMS}"
            ${_pebble_args}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE _pebble_result
  )
  if(_pebble_result)
    message(FATAL_ERROR "Could not configure the project from package.json")
  endif()
  include(${CMAKE_BINARY_DIR}/pebble/project.cmake)
  set(PEBBLE_PROJECT_JSON ${CMAKE_BINARY_DIR}/pebble/project.json)
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${PEBBLE_CONFIGURE_DEPENDS} ${_PEBBLE_TOOL})
  cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL _pebble_finalize)
endmacro()

function(_pebble_expect_type caller)
  if(NOT PEBBLE_PROJECT_TYPE IN_LIST ARGN)
    message(FATAL_ERROR
      "${caller}() does not apply to a '${PEBBLE_PROJECT_TYPE}' project "
      "(package.json projectType)")
  endif()
  if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "${caller}() has to be called from the top-level CMakeLists.txt")
  endif()
endfunction()

# What every binary for a platform shares: flags, the SDK, the packages the
# project uses, and the resources.
function(_pebble_platform p)
  if(TARGET pebble_${p})
    return()
  endif()
  set(build ${CMAKE_BINARY_DIR})
  set(sdk ${PEBBLE_${p}_SDK_DIR})

  add_library(pebble_${p} INTERFACE)
  target_compile_options(pebble_${p} INTERFACE
    -mcpu=cortex-m3 -mthumb -fPIE -g
    -ffunction-sections -fdata-sections -fcommon
    -Wall -Wextra -Wno-unused-parameter
    $<$<BOOL:${PEBBLE_WARNINGS_AS_ERRORS}>:-Werror>
  )
  target_compile_definitions(pebble_${p} INTERFACE
    ${PEBBLE_${p}_DEFINES}
    $<IF:$<CONFIG:Debug>,PBL_DEBUG,RELEASE>
    $<$<BOOL:${PEBBLE_${p}_OWN_TIME_H}>:_TIME_H_;time_t=long>
  )
  target_include_directories(pebble_${p} INTERFACE
    ${sdk}/include
    ${build}/${p}
    ${build}/include
    ${PEBBLE_${p}_PACKAGE_INCLUDES}
  )
  target_link_options(pebble_${p} INTERFACE
    -mcpu=cortex-m3 -mthumb -fPIE
    -Wl,--gc-sections -Wl,--warn-common
    -Wl,--build-id=sha1 -Wl,--emit-relocs
    -T${build}/${p}/pebble_app.ld
    $<$<NOT:$<BOOL:${PEBBLE_${p}_FROZEN}>>:-nostartfiles>
  )
  # Packages call each other and the SDK in whatever order they like.
  target_link_libraries(pebble_${p} INTERFACE
    -Wl,--start-group ${sdk}/lib/libpebble.a ${PEBBLE_${p}_PACKAGE_ARCHIVES} -Wl,--end-group
  )

  set(outputs ${build}/${p}/src/resource_ids.auto.h ${build}/${p}/src/resource_ids.auto.c
              ${build}/${p}/resources.resball)
  if(PEBBLE_PROJECT_TYPE STREQUAL "package")
    list(APPEND outputs ${build}/${p}/package_resources.json)
  else()
    list(APPEND outputs ${build}/${p}/app_resources.pbpack)
  endif()
  add_custom_command(
    OUTPUT ${outputs}
    COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} resources
            --project ${PEBBLE_PROJECT_JSON} --platform ${p}
    DEPENDS ${PEBBLE_PROJECT_JSON} ${PEBBLE_${p}_RESOURCE_INPUTS} ${_PEBBLE_TOOL}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ${p} resources"
    VERBATIM
  )
  add_custom_target(pebble_resources_${p} DEPENDS ${outputs})
endfunction()

# app_<platform> or worker_<platform>, out of the `app` or `worker`
# interface.
function(_pebble_binary kind)
  set(build ${CMAKE_BINARY_DIR})
  foreach(p ${PEBBLE_PLATFORMS})
    _pebble_platform(${p})
    # The generated sources link after the project's own.
    add_library(pebble_${kind}_${p}_generated INTERFACE)
    target_sources(pebble_${kind}_${p}_generated INTERFACE
      ${build}/${p}/appinfo.auto.c
      ${build}/${p}/src/resource_ids.auto.c
      ${build}/${p}/src/resource_ids.auto.h
      $<$<BOOL:${PEBBLE_MESSAGE_KEYS}>:${build}/src/message_keys.auto.c>
    )
    add_executable(${kind}_${p})
    target_link_libraries(${kind}_${p} PRIVATE ${kind} pebble_${kind}_${p}_generated pebble_${p})
    target_link_options(${kind}_${p} PRIVATE -Wl,-Map=${build}/${p}/pebble-${kind}.map)
    set_target_properties(${kind}_${p} PROPERTIES
      OUTPUT_NAME pebble-${kind}
      SUFFIX .elf
      RUNTIME_OUTPUT_DIRECTORY ${build}/${p}
      LINK_DEPENDS ${build}/${p}/pebble_app.ld
    )
    add_dependencies(${kind}_${p} pebble_resources_${p})
  endforeach()
endfunction()

function(pebble_app)
  cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
  _pebble_expect_type(pebble_app app moddable)
  add_library(app INTERFACE)
  target_sources(app INTERFACE ${ARG_SOURCES})
  _pebble_binary(app)
endfunction()

function(pebble_worker)
  cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
  _pebble_expect_type(pebble_worker app moddable)
  add_library(worker INTERFACE)
  target_sources(worker INTERFACE ${ARG_SOURCES})
  _pebble_binary(worker)
endfunction()

function(pebble_js)
  cmake_parse_arguments(ARG "" "ENTRY" "SOURCES" ${ARGN})
  _pebble_expect_type(pebble_js app moddable plugin package)
  foreach(source ${ARG_SOURCES})
    get_filename_component(source ${source} ABSOLUTE)
    set_property(GLOBAL APPEND PROPERTY PEBBLE_JS_SOURCES ${source})
  endforeach()
  if(ARG_ENTRY)
    get_filename_component(entry ${ARG_ENTRY} ABSOLUTE)
    set_property(GLOBAL PROPERTY PEBBLE_JS_ENTRY ${entry})
  endif()
endfunction()

function(pebble_package)
  cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS" ${ARGN})
  _pebble_expect_type(pebble_package package)
  set(build ${CMAKE_BINARY_DIR})
  add_library(package INTERFACE)
  target_sources(package INTERFACE ${ARG_SOURCES})
  foreach(header ${ARG_HEADERS})
    get_filename_component(header ${header} ABSOLUTE)
    set_property(GLOBAL APPEND PROPERTY PEBBLE_PACKAGE_HEADERS ${header})
  endforeach()

  # Scoped packages (@scope/name) keep their archives under the scope.
  string(REGEX MATCH "[^/]+$" name ${PEBBLE_PROJECT_NAME})
  string(REGEX MATCH "^@[^/]+" scope ${PEBBLE_PROJECT_NAME})
  foreach(p ${PEBBLE_PLATFORMS})
    _pebble_platform(${p})
    add_library(package_${p} STATIC ${build}/${p}/src/resource_ids.auto.h)
    target_link_libraries(package_${p} PRIVATE package pebble_${p})
    set_target_properties(package_${p} PROPERTIES
      OUTPUT_NAME ${name}
      ARCHIVE_OUTPUT_DIRECTORY ${build}/${p}/${scope}
    )
    add_dependencies(package_${p} pebble_resources_${p})
  endforeach()
endfunction()

# PebbleKit JS, as the files that go into the pbw or package.
function(_pebble_js_outputs out)
  set(build ${CMAKE_BINARY_DIR})
  get_property(sources GLOBAL PROPERTY PEBBLE_JS_SOURCES)
  if(NOT sources)
    set(${out} "" PARENT_SCOPE)
    return()
  endif()

  if(PEBBLE_ENABLE_MULTI_JS AND NOT PEBBLE_PROJECT_TYPE STREQUAL "package")
    get_property(entry GLOBAL PROPERTY PEBBLE_JS_ENTRY)
    if(NOT entry AND PEBBLE_JS_ENTRY)
      set(entry ${CMAKE_SOURCE_DIR}/${PEBBLE_JS_ENTRY})
    elseif(NOT entry AND EXISTS ${CMAKE_SOURCE_DIR}/src/pkjs/index.js)
      set(entry ${CMAKE_SOURCE_DIR}/src/pkjs/index.js)
    elseif(NOT entry)
      set(entry ${CMAKE_SOURCE_DIR}/src/js/app.js)
    endif()
    find_program(PEBBLE_NODE node REQUIRED)
    set(outputs ${build}/pebble-js-app.js ${build}/pebble-js-app.js.map)
    set(depends ${sources} ${PEBBLE_PACKAGE_JS})
    if(PEBBLE_MESSAGE_KEYS)
      list(APPEND depends ${build}/js/message_keys.json)
    endif()
    add_custom_command(
      OUTPUT ${outputs}
      COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} js
              --project ${PEBBLE_PROJECT_JSON}
              --entry ${entry}
              --output ${build}/pebble-js-app.js
              --node ${PEBBLE_NODE}
              --node-modules ${PEBBLE_NODE_MODULES}
              ${sources}
      DEPENDS ${depends} ${PEBBLE_PROJECT_JSON}
      COMMENT "Bundling PebbleKit JS"
      VERBATIM
    )
  else()
    # Copied over as they are, src/ dropped from their paths.
    set(outputs "")
    foreach(source ${sources})
      file(RELATIVE_PATH rel ${CMAKE_SOURCE_DIR}/src ${source})
      if(rel MATCHES "^\\.\\./")
        file(RELATIVE_PATH rel ${CMAKE_SOURCE_DIR} ${source})
      endif()
      list(APPEND outputs ${build}/${rel})
    endforeach()
    add_custom_command(
      OUTPUT ${outputs}
      COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} js --project ${PEBBLE_PROJECT_JSON} ${sources}
      DEPENDS ${sources} ${PEBBLE_PROJECT_JSON}
      COMMENT "Copying JS"
      VERBATIM
    )
  endif()
  set(${out} ${outputs} PARENT_SCOPE)
endfunction()

function(_pebble_finalize)
  set(build ${CMAKE_BINARY_DIR})
  set(timestamp ${build}/pebble/timestamp)
  # inject_metadata runs the binutils by name.
  get_filename_component(toolchain_bin ${CMAKE_C_COMPILER} DIRECTORY)

  if(PEBBLE_PROJECT_TYPE STREQUAL "package")
    if(NOT TARGET package)
      message(FATAL_ERROR "A Pebble package project has to call pebble_package()")
    endif()
    get_property(headers GLOBAL PROPERTY PEBBLE_PACKAGE_HEADERS)
    get_property(js_sources GLOBAL PROPERTY PEBBLE_JS_SOURCES)
    set(depends ${headers} ${js_sources} ${PEBBLE_PROJECT_JSON})
    foreach(p ${PEBBLE_PLATFORMS})
      list(APPEND depends package_${p} ${build}/${p}/package_resources.json)
    endforeach()
    add_custom_command(
      OUTPUT ${CMAKE_SOURCE_DIR}/dist.zip
      COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} package
              --project ${PEBBLE_PROJECT_JSON}
              --output ${CMAKE_SOURCE_DIR}/dist.zip
              --headers ${headers}
              --js ${js_sources}
      DEPENDS ${depends}
      COMMENT "Packaging dist.zip"
      VERBATIM
    )
    add_custom_target(pebble_package_zip ALL DEPENDS ${CMAKE_SOURCE_DIR}/dist.zip)
    return()
  endif()

  if(NOT PEBBLE_PROJECT_TYPE STREQUAL "plugin")
    if(NOT TARGET app)
      message(FATAL_ERROR "An app project has to call pebble_app()")
    endif()
    get_target_property(app_sources app INTERFACE_SOURCES)
    if(NOT app_sources)
      message(FATAL_ERROR "pebble_app() was given no sources")
    endif()
  endif()

  _pebble_js_outputs(js)

  set(has_js "")
  if(js)
    set(has_js --has-js)
  endif()
  set(has_worker "")
  if(TARGET worker)
    set(has_worker --has-worker)
  endif()

  # The microphone permission check reads the app's sources, once.
  file(GENERATE OUTPUT ${build}/pebble/app_sources.txt
    CONTENT "$<$<TARGET_EXISTS:app>:$<TARGET_PROPERTY:app,INTERFACE_SOURCES>>")
  set(sources_list --sources-list ${build}/pebble/app_sources.txt)

  set(bundle_depends ${js} ${build}/appinfo.json ${PEBBLE_BUNDLED_FILES})
  set(manifest "")
  foreach(p ${PEBBLE_PLATFORMS})
    set(resources ${build}/${p}/app_resources.pbpack)
    set(entry "\"app\": \"${build}/${p}/pebble-app.bin\", \"resources\": \"${resources}\"")
    foreach(kind app worker)
      if(NOT TARGET ${kind}_${p})
        continue()
      endif()
      set(bin ${build}/${p}/pebble-${kind}.bin)
      set(extra "")
      if(kind STREQUAL "app")
        set(extra --resources ${resources} ${sources_list})
        set(sources_list "")
      else()
        string(APPEND entry ", \"worker\": \"${bin}\"")
      endif()
      add_custom_command(
        OUTPUT ${bin}
        BYPRODUCTS ${build}/${p}/pebble-${kind}.raw.bin
        COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} binary
                --project ${PEBBLE_PROJECT_JSON} --platform ${p} --kind ${kind}
                --elf $<TARGET_FILE:${kind}_${p}> --output ${bin}
                --timestamp ${timestamp} ${has_js} ${has_worker} ${extra}
                --objcopy ${CMAKE_OBJCOPY}
                --toolchain-bin ${toolchain_bin}
        DEPENDS ${kind}_${p} ${resources} ${_PEBBLE_TOOL}
        COMMENT "Generating ${p} ${kind} binary"
        VERBATIM
      )
      list(APPEND bundle_depends ${bin} ${resources})
    endforeach()
    list(APPEND manifest "\"${p}\": {${entry}}")
  endforeach()

  list(JOIN manifest ", " manifest)
  list(TRANSFORM js REPLACE "(.+)" "\"\\1\"" OUTPUT_VARIABLE js_json)
  list(JOIN js_json ", " js_json)
  file(GENERATE OUTPUT ${build}/pebble/bundle.json
    CONTENT "{\"platforms\": {${manifest}}, \"js\": [${js_json}]}\n")

  get_filename_component(pbw_name ${CMAKE_SOURCE_DIR} NAME)
  set(pbw ${build}/${pbw_name}.pbw)
  add_custom_command(
    OUTPUT ${pbw}
    COMMAND ${PEBBLE_PYTHON} ${_PEBBLE_TOOL} bundle
            --project ${PEBBLE_PROJECT_JSON} --manifest ${build}/pebble/bundle.json
            --timestamp ${timestamp} --output ${pbw}
    DEPENDS ${bundle_depends} ${build}/pebble/bundle.json ${PEBBLE_PROJECT_JSON}
    COMMENT "Bundling ${pbw_name}.pbw"
    VERBATIM
  )
  add_custom_target(pebble_pbw ALL DEPENDS ${pbw})
endfunction()
