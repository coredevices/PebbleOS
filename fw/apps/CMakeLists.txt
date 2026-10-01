# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(demo)
add_subdirectory(system)
if(CONFIG_SHELL_SDK)
  add_subdirectory(sdk)
else()
  add_subdirectory(watch)
endif()
