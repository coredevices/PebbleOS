# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_compile_definitions(
  USE_HAL_DRIVER
  SOC_BF0_HCPU
  HAL_TICK_PER_SECOND=1024
  HAL_NMI_HANLDER_OVERRIDED
)
pbl_link_options(-Wl,--undefined=HAL_GetTick)
if(CONFIG_HAL_SIFLI_SF32LB52)
  pbl_compile_definitions(
    SF32LB52X
  )
endif()
set(micro_sources
  SiFli-SDK/drivers/hal/bf0_hal.c
  SiFli-SDK/drivers/hal/bf0_hal_hlp.c
  SiFli-SDK/drivers/hal/bf0_hal_pmu.c
  SiFli-SDK/drivers/hal/bf0_hal_rcc.c
)
set(ramfunc_sources
  SiFli-SDK/drivers/hal/bf0_hal_rcc.c
)
if(CONFIG_HAL_SIFLI_AON)
  list(APPEND micro_sources
    SiFli-SDK/drivers/hal/bf0_hal_hpaon.c
    SiFli-SDK/drivers/hal/bf0_hal_lpaon.c
  )
  list(APPEND ramfunc_sources
    SiFli-SDK/drivers/hal/bf0_hal_hpaon.c
  )
endif()
if(CONFIG_HAL_SIFLI_LCPU_CONFIG)
  list(APPEND micro_sources
    SiFli-SDK/drivers/hal/bf0_hal_lcpu_config.c
  )
endif()
if(CONFIG_HAL_SIFLI_LCPU_PATCH)
  list(APPEND micro_sources
    SiFli-SDK/drivers/hal/bf0_hal_patch.c
  )
endif()
if(CONFIG_HAL_SIFLI_SF32LB52)
  list(APPEND micro_sources
    sf32lb52/system_bf0_ap.c
  )
  if(CONFIG_HAL_SIFLI_LCPU_CONFIG)
    list(APPEND micro_sources
      SiFli-SDK/drivers/cmsis/sf32lb52x/lcpu_config_type.c
    )
  endif()
  if(CONFIG_HAL_SIFLI_LCPU)
    list(APPEND micro_sources
      ../nonfree/pebbleos-nonfree/sf32lb52/lcpu_patch_rev_b.c
      SiFli-SDK/drivers/cmsis/sf32lb52x/bt_rf_fulcal.c
    )
  endif()
endif()
set(micro_includes
  SiFli-SDK/customer/boards/include
  SiFli-SDK/drivers/Include
  SiFli-SDK/drivers/cmsis/Include
  SiFli-SDK/external/CMSIS/Include
  SiFli-SDK/middleware/include
)
if(CONFIG_HAL_SIFLI_SF32LB52)
  list(APPEND micro_includes
    sf32lb52
    SiFli-SDK/drivers/cmsis/sf32lb52x
  )
endif()
pbl_include_directories(${micro_includes})
pbl_library_named(hal_sifli)
pbl_library_kind(stlib)
pbl_library_sources(${micro_sources})
pbl_library_ramfunc(${ramfunc_sources})
pbl_library_compile_options(-Wno-unused-value -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-sign-compare -Wno-type-limits)
