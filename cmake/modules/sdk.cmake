# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# The shippable SDK. pbl_native_sdk() generates the headers and libpebble.a
# into build/sdk/<abi>; this rounds that out into a complete SDK with the
# description of the platform, the project templates, the shared tools and
# the waf app developers build with. See docs/sdk/building.md.

function(pbl_sdk)
  # Packaging is asked for explicitly and copies a few hundred files, so it
  # repackages wholesale instead of tracking each one.
  add_custom_target(sdk
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/sdk.py
            --output ${PROJECT_BINARY_DIR}/sdk --platform ${PBL_PLATFORM_NAME}
            --abi ${CONFIG_PLATFORM_ABI}
            --work-dir ${PROJECT_BINARY_DIR}/sdk-waf
    # What the app build knows the platform by.
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/platform_config.py
            --dotconfig ${PBL_DOTCONFIG}
            --json ${PROJECT_BINARY_DIR}/sdk/${PBL_PLATFORM_NAME}/platform.json
    DEPENDS pbl_native_sdk
    COMMENT "Packaging the SDK into ${PROJECT_BINARY_DIR}/sdk"
    VERBATIM
  )
endfunction()
