# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_include_directories(.)

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
add_subdirectory(apps/core)

if(CONFIG_RECOVERY_FW)
  add_subdirectory(apps/prf)
else()
  add_subdirectory(apps)
endif()

file(GLOB sources CONFIGURE_DEPENDS *.c)
if(CONFIG_ARCH_POSIX)
  pbl_filter_sources(sources EXCLUDE hardfault_handler.c)
endif()
pbl_library()
pbl_library_sources(${sources})
# A native build's main() is the host process entry; the firmware's own
# one is entered from the kernel thread it hands over to.
if(CONFIG_ARCH_POSIX)
  set_property(SOURCE main.c APPEND PROPERTY COMPILE_DEFINITIONS main=pbl_fw_main)
endif()

if(NOT CONFIG_RECOVERY_FW)
  # One universal catalog, independent of board and enabled services.
  file(GLOB_RECURSE pot_sources CONFIGURE_DEPENDS *.c *.h *.def)
  pbl_firmware_pot(${PROJECT_BINARY_DIR}/pebbleos.pot ${pot_sources}
    EXCLUDE apps/prf/**)
endif()
