# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_TEMPERATURE_NRF5)
  list(APPEND sources nrf5.c)
elseif(CONFIG_TEMPERATURE_SF32LB)
  list(APPEND sources sf32lb.c)
elseif(CONFIG_TEMPERATURE_QEMU)
  list(APPEND sources qemu.c)
elseif(CONFIG_TEMPERATURE_POSIX)
  list(APPEND sources posix.c)
endif()
if(CONFIG_SHELL AND NOT CONFIG_RECOVERY_FW)
  list(APPEND sources shell.c)
endif()
pbl_library()
pbl_library_sources(${sources})
