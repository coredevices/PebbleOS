# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(.)
pbl_library_sources(
  init.c
  reset.c
  sleep.c
  syscfg.c
  idle.c
  lpsys.c
  lpsys_halt.c
  mpu_regions.c
)
pbl_linker_sources(ramfunc ramfunc.ld)

if(CONFIG_SF32LB52_FTAB)
  set(ftab ${PROJECT_BINARY_DIR}/ftab.hex)
  math(EXPR image_offset "${CONFIG_FLASH_OFFSET} + ${CONFIG_FIRMWARE_OFFSET}")
  math(EXPR image_size "${CONFIG_FW_FLASH_SIZE} - ${CONFIG_FIRMWARE_OFFSET}")
  add_custom_command(OUTPUT ${ftab}
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/sf32lb52_ftab.py
            --dtb ${PBL_DTB}
            --flash-base ${CONFIG_FLASH_BASE}
            --image-offset ${image_offset}
            --image-size ${image_size}
            --output ${ftab}
    DEPENDS ${PBL_DTB} ${PBL_BASE}/tools/cmake/sf32lb52_ftab.py
            ${PBL_BASE}/tools/devicetree/fdt.py
    COMMENT "Generating the boot ROM flash table"
    VERBATIM)
  add_custom_target(pbl_ftab ALL DEPENDS ${ftab})
endif()
