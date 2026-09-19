# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(. applib/vendor/uPNG applib/vendor/tinflate)

# The firmware reports the version it was built from.
set(git_version ${CMAKE_CURRENT_BINARY_DIR}/git_version.auto.h)
add_custom_command(
  OUTPUT ${git_version} ${CMAKE_CURRENT_BINARY_DIR}/git_version.stamp
  COMMAND ${PBL_TOOLCHAIN_ENV} ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/generate.py git-version
          --template ${CMAKE_CURRENT_SOURCE_DIR}/git_version.auto.h.in
          --output ${git_version}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/git_version.auto.h.in
  WORKING_DIRECTORY ${PBL_BASE}
  COMMENT "Checking the firmware version"
  VERBATIM
)
# The stamp is never written, so the version is re-checked on every build;
# the header itself is only rewritten when it actually changes.
add_custom_target(pbl_git_version DEPENDS ${git_version})
add_dependencies(pbl_generated_headers pbl_git_version)

add_subdirectory(startup)
add_subdirectory(drivers)
add_subdirectory(board)
add_subdirectory(shell)
add_subdirectory(services)
add_subdirectory(applib)
add_subdirectory(mfg)
add_subdirectory(comm)
add_subdirectory(console)
add_subdirectory(debug)
add_subdirectory(flash_region)
add_subdirectory(kernel)
add_subdirectory(popups)
add_subdirectory(process_management)
add_subdirectory(process_state)
add_subdirectory(resource)
add_subdirectory(syscall)
add_subdirectory(system)
add_subdirectory(util)
add_subdirectory(apps/core)

if(CONFIG_RECOVERY_FW)
  add_subdirectory(apps/prf)
else()
  add_subdirectory(apps)
endif()

file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_library()
pbl_library_sources(${sources})

if(NOT CONFIG_RECOVERY_FW)
  # One universal catalog, independent of board and enabled services.
  file(GLOB_RECURSE pot_sources CONFIGURE_DEPENDS *.c *.h *.def)
  pbl_firmware_pot(${PROJECT_BINARY_DIR}/pebbleos.pot ${pot_sources}
    EXCLUDE apps/prf/** applib/vendor/**)
endif()
