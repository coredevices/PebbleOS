# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The images a build flashes: the application, and those an application adds
# to boot with, such as its bootloader, from Intel HEX files placed in a flash
# partition. They flash in the order added, the application last; the first
# is the one the boot ROM starts. pbl reads them from images.yaml.

# Adds image NAME, the Intel HEX file HEX, which has to fit partition
# PARTITION of the flash the application runs from.
function(pbl_image name)
  cmake_parse_arguments(arg "" "HEX;PARTITION" "" ${ARGN})
  if(NOT arg_HEX OR NOT arg_PARTITION)
    message(FATAL_ERROR "pbl_image(${name}) needs HEX and PARTITION")
  endif()
  if(NOT EXISTS ${arg_HEX})
    message(FATAL_ERROR "pbl_image(${name}): no ${arg_HEX}; is its submodule initialized?")
  endif()
  set_property(GLOBAL APPEND PROPERTY PBL_IMAGES ${name})
  set_property(GLOBAL PROPERTY PBL_IMAGE_${name}_HEX ${arg_HEX})
  set_property(GLOBAL PROPERTY PBL_IMAGE_${name}_PARTITION ${arg_PARTITION})
endfunction()

# Adds FILE to those flashed with image NAME, such as a table the boot ROM
# reads to start it.
function(pbl_image_add_file name file)
  set_property(GLOBAL APPEND PROPERTY PBL_IMAGE_${name}_FILES ${file})
endfunction()

# Writes images.yaml, the application's image being APP_HEX. The SoC adds
# what its boot ROM needs to the first image through pbl_soc_boot_image(NAME
# PARTITION), PARTITION being empty for the application.
function(pbl_images app_hex)
  get_property(images GLOBAL PROPERTY PBL_IMAGES)
  set(outputs)
  foreach(name ${images})
    get_property(hex GLOBAL PROPERTY PBL_IMAGE_${name}_HEX)
    get_property(partition GLOBAL PROPERTY PBL_IMAGE_${name}_PARTITION)
    set(out ${PROJECT_BINARY_DIR}/images/${name}.hex)
    add_custom_command(OUTPUT ${out}
      COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/images.py
              --dtb ${PBL_DTB}
              --flash-base ${CONFIG_FLASH_BASE}
              --flash-size ${CONFIG_FLASH_SIZE}
              --partition ${partition}
              --input ${hex}
              --output ${out}
      DEPENDS ${hex} ${PBL_DTB} ${PBL_BASE}/tools/cmake/images.py
              ${PBL_BASE}/tools/devicetree/dtmap.py ${PBL_BASE}/tools/devicetree/fdt.py
      COMMENT "Placing the ${name} image"
      VERBATIM)
    list(APPEND outputs ${out})
    pbl_image_add_file(${name} ${out})
  endforeach()
  pbl_image_add_file(app ${app_hex})

  set(all ${images} app)
  list(GET all 0 boot)
  get_property(boot_partition GLOBAL PROPERTY PBL_IMAGE_${boot}_PARTITION)
  if(COMMAND pbl_soc_boot_image)
    pbl_soc_boot_image(${boot} "${boot_partition}")
  endif()

  set(yaml "images:\n")
  foreach(name ${all})
    get_property(files GLOBAL PROPERTY PBL_IMAGE_${name}_FILES)
    string(APPEND yaml "  - name: ${name}\n    files:\n")
    foreach(file ${files})
      string(APPEND yaml "      - ${file}\n")
    endforeach()
  endforeach()
  # Only written when it changes
  file(CONFIGURE OUTPUT ${PROJECT_BINARY_DIR}/images.yaml CONTENT "${yaml}" @ONLY)

  if(outputs)
    add_custom_target(pbl_images ALL DEPENDS ${outputs})
  endif()
endfunction()
