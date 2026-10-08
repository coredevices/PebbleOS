# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library_sources(mcu.c)
if(CONFIG_ARCH_POSIX_FIBERS)
  pbl_library_sources(fiber.c)
else()
  pbl_library_sources(
    arch.c
    native.c
  )
endif()

pbl_include_directories(include)
