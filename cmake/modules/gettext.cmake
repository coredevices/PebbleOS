# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Universal source catalog consumed by the translation service and
# the tools in pebbleos-translations.

find_program(XGETTEXT xgettext REQUIRED)

# gettext >= 0.22 shells out to git for a reproducible POT-Creation-Date,
# which warns once per source file on out-of-tree build paths.
execute_process(COMMAND ${XGETTEXT} --help OUTPUT_VARIABLE xgettext_help)
if(xgettext_help MATCHES "--no-git")
  set(XGETTEXT_NO_GIT --no-git)
else()
  set(XGETTEXT_NO_GIT "")
endif()

set(PBL_GETTEXT_KEYWORDS
  i18n_noop
  i18n_get
  i18n_get_with_buffer
  sys_i18n_get_with_buffer
  i18n_ctx_noop:1c,2
  i18n_ctx_get:1c,2
  i18n_ctx_get_with_buffer:1c,2
)

define_property(GLOBAL PROPERTY PBL_POT_TARGET
  BRIEF_DOCS "The target that builds the universal firmware .pot")

function(pbl_pot_target output var)
  file(RELATIVE_PATH relative ${PROJECT_BINARY_DIR} ${output})
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" relative ${relative})
  set(${var} pbl_pot_${relative} PARENT_SCOPE)
endfunction()

function(pbl_gettext output)
  set(sources ${ARGN})
  list(SORT sources)
  set(keyword_args "")
  foreach(keyword ${PBL_GETTEXT_KEYWORDS})
    list(APPEND keyword_args --keyword=${keyword})
  endforeach()

  # xgettext echoes the paths it is given into the #: references, so they
  # are passed relative to the repository: an absolute path would put the
  # build machine's directory layout in the catalogs.
  set(relative_sources "")
  foreach(source ${sources})
    file(RELATIVE_PATH source ${PBL_BASE} ${source})
    list(APPEND relative_sources ${source})
  endforeach()

  # The source list is long enough to overflow a command line.
  set(list_file ${output}.files)
  string(REPLACE ";" "\n" file_list "${relative_sources}")
  file(CONFIGURE OUTPUT ${list_file} CONTENT "${file_list}\n" @ONLY)

  add_custom_command(
    OUTPUT ${output}
    COMMAND ${XGETTEXT} ${XGETTEXT_NO_GIT} -c/ -k --from-code=UTF-8
            --language=C ${keyword_args} -o ${output} --files-from=${list_file}
    DEPENDS ${sources} ${list_file}
    WORKING_DIRECTORY ${PBL_BASE}
    COMMENT "Extracting strings into ${output}"
    VERBATIM
  )

  pbl_pot_target(${output} target)
  add_custom_target(${target} DEPENDS ${output})
endfunction()

# The universal catalog is built along with normal firmware.
function(pbl_firmware_pot output)
  cmake_parse_arguments(ARG "" "" "EXCLUDE" ${ARGN})
  set(sources ${ARG_UNPARSED_ARGUMENTS})
  pbl_filter_sources(sources EXCLUDE ${ARG_EXCLUDE})
  pbl_gettext(${output} ${sources})
  pbl_pot_target(${output} target)
  set_property(GLOBAL PROPERTY PBL_POT_TARGET ${target})
endfunction()
