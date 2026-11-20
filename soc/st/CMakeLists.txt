# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(include)

if(CONFIG_SOC_STM32H7)
  add_subdirectory(stm32h7)
endif()
