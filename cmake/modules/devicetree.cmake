# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Generates the instance file of each driver, which is empty if the board has
# no such device, devicetree/devices.h, and devicetree/roles.c, built into the
# library calling this. A board's tree is compiled, and the roles files of the
# board and the application merged, by tools/cmake/kconfig.py before Kconfig
# runs. Files that do not change keep
# their time, so the stamp is what tells when they were last generated.

function(pbl_devicetree)
  set(tool ${PBL_BASE}/tools/devicetree)
  set(outdir ${CMAKE_BINARY_DIR}/generated/include)
  set(stamp ${CMAKE_BINARY_DIR}/devicetree/generated.stamp)
  set(outputs ${outdir}/devicetree/devices.h)
  foreach(output ${PBL_DT_OUTPUTS})
    list(APPEND outputs ${outdir}/${output})
  endforeach()
  set(roles ${CMAKE_BINARY_DIR}/devicetree/roles.yml)
  set(dtb_arg "")
  if(PBL_DTB)
    set(dtb_arg --dtb ${PBL_DTB})
  endif()
  add_custom_command(
    OUTPUT ${stamp}
    BYPRODUCTS ${outputs}
    COMMAND ${PYTHON_EXECUTABLE} ${tool}/dtmap.py
            --srcdir ${PBL_BASE}
            --appdir ${PBL_APPLICATION_SOURCE_DIR}
            ${dtb_arg}
            --config ${PBL_DOTCONFIG}
            --roles ${roles}
            --outdir ${outdir}
    COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
    DEPENDS ${PBL_DTB} ${PBL_DOTCONFIG} ${roles} ${PBL_KCONFIG_DEPENDS} ${tool}/dtmap.py ${tool}/fdt.py
    COMMENT "Generating the board devices from devicetree"
    VERBATIM
  )
  add_custom_target(pbl_devicetree DEPENDS ${stamp})
  add_dependencies(pbl_generated_headers pbl_devicetree)
  pbl_library_sources(${outdir}/devicetree/roles.c)
endfunction()
