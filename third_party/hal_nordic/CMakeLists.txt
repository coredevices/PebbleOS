# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_HAL_NORDIC_NRF52840)
  pbl_compile_definitions(NRF52840_XXAA)
endif()
pbl_include_directories(
  .
  nrfx
  nrfx/hal
  nrfx/helpers
  nrfx/mdk
  nrfx/drivers/include
  nrfx/templates
)
