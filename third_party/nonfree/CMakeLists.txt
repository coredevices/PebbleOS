# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(nonfree pebbleos-nonfree)

if(CONFIG_NRF_FUEL_GAUGE)
  if(CONFIG_CPU_CORTEX_M4)
    set(fuel_gauge_lib ${nonfree}/npm1300/nrf_fuel_gauge/lib/cortex-m4/soft-float)
  elseif(CONFIG_CPU_CORTEX_M33)
    set(fuel_gauge_lib ${nonfree}/npm1300/nrf_fuel_gauge/lib/cortex-m33/soft-float)
  endif()
  pbl_include_directories(
    ${nonfree}/npm1300/nrf_fuel_gauge/include
    ${nonfree}/npm1300/models
  )
  pbl_library_import(nrf_fuel_gauge ${fuel_gauge_lib})
endif()

if(CONFIG_CST816_FW)
  if(CONFIG_BOARD_OBELIX)
    pbl_include_directories(${nonfree}/cst816/obelix)
  elseif(CONFIG_BOARD_GETAFIX)
    pbl_include_directories(${nonfree}/cst816/getafix)
  else()
    message(FATAL_ERROR "no CST816 firmware for board ${PBL_BOARD}")
  endif()
endif()

if(CONFIG_HRM_BACKEND_GOODIX)
  set(gh3x2x ${nonfree}/gh3x2x)
  pbl_library_import(star-mc1_arm-gcc14.2.1_softfp+Os_common_dl ${gh3x2x}/algo_lib/COMMON_DL)
  pbl_library_import(star-mc1_arm-gcc14.2.1_softfp+Os_common_dsp ${gh3x2x}/algo_lib/COMMON_DSP)
  pbl_library_import("GitInfo_star-mc1-arm-none-eabi-gcc1421(softfp+Os)" ${gh3x2x}/algo_lib/HR)
  pbl_library_import(star-mc1_arm-gcc14.2.1_softfp+Os_hrv ${gh3x2x}/algo_lib/HRV)
  pbl_library_import(star-mc1_arm-gcc14.2.1_softfp+Os_nadt ${gh3x2x}/algo_lib/NADT)
  pbl_library_import(star-mc1_arm-gcc14.2.1_softfp+Os_spo2 ${gh3x2x}/algo_lib/SPO2)

  file(GLOB sources CONFIGURE_DEPENDS
    ${gh3x2x}/algo_lib/algo_params/HR/04_EXCLUSIVE/*.c
    ${gh3x2x}/algo_lib/algo_params/SPO2/*.c
  )
  pbl_library_named(hrm_goodix)
  pbl_library_sources(
    ${sources}
    ${gh3x2x}/algo_lib/algo_params/goodix_hrv_config.c
    ${gh3x2x}/algo_lib/algo_params/goodix_nadt_config.c
    ${gh3x2x}/pebbleos/hrm_goodix.c
  )
  pbl_library_include_directories(
    ${gh3x2x}/algo_lib/inc
    ${gh3x2x}/algo_lib/algo_params/HR/04_EXCLUSIVE
  )
  pbl_library_compile_options(-Wno-sign-compare -Wno-type-limits)
endif()
