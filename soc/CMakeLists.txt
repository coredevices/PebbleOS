# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_QEMU)
  add_subdirectory(qemu)
elseif(CONFIG_SOC_NRF52)
  add_subdirectory(nrf)
elseif(CONFIG_SOC_FAMILY_SF32LB)
  add_subdirectory(sf32lb)
elseif(CONFIG_SOC_POSIX)
  add_subdirectory(posix)
endif()
