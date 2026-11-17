# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_include_directories(include)

if(CONFIG_SOC_SF32LB52)
  add_subdirectory(sf32lb52x)
elseif(CONFIG_SOC_SF32LB57)
  add_subdirectory(sf32lb57x)
endif()

# The boot ROM starts the image the flash table points it at: the first of the
# build, the bootloader of the application or the application itself.
if(CONFIG_SF32LB_FTAB)
  function(pbl_soc_boot_image name partition)
    set(ftab ${PROJECT_BINARY_DIR}/ftab.hex)
    if(partition)
      set(boot_args --boot-partition ${partition})
    else()
      math(EXPR image_offset "${CONFIG_FLASH_OFFSET} + ${CONFIG_FIRMWARE_OFFSET}")
      math(EXPR image_size "${CONFIG_FW_FLASH_SIZE} - ${CONFIG_FIRMWARE_OFFSET}")
      set(boot_args --image-offset ${image_offset} --image-size ${image_size})
    endif()
    add_custom_command(OUTPUT ${ftab}
      COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/sf32lb_ftab.py
              --dtb ${PBL_DTB}
              --flash-base ${CONFIG_FLASH_BASE}
              ${boot_args}
              --output ${ftab}
      DEPENDS ${PBL_DTB} ${PBL_BASE}/tools/cmake/sf32lb_ftab.py
              ${PBL_BASE}/tools/devicetree/dtmap.py ${PBL_BASE}/tools/devicetree/fdt.py
      COMMENT "Generating the boot ROM flash table"
      VERBATIM)
    add_custom_target(pbl_ftab ALL DEPENDS ${ftab})
    pbl_image_add_file(${name} ${ftab})
  endfunction()
endif()
