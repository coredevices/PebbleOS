# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_library()
pbl_library_sources(${sources})

add_subdirectory(services)
