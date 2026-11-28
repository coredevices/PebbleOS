# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ARCH_POSIX)
  add_subdirectory(posix)
elseif(CONFIG_RISCV)
  add_subdirectory(riscv)
else()
  add_subdirectory(arm)
endif()
