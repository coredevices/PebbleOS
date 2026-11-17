# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Board- and platform-level facts derived from the configuration: the SDK
# platform the firmware exposes.

# The platform is the one of Kconfig.platform the board picked. Tools are
# told it by name, and find what it is like in tools/pebble_sdk_platform.py.
if(NOT CONFIG_PLATFORM_NAME)
  message(FATAL_ERROR "No platform specified for ${PBL_BOARD}")
endif()
set(PBL_PLATFORM_NAME ${CONFIG_PLATFORM_NAME})

# The oldest SDK whose apps run: 2.x apps are black and white, and
# rectangular.
if(CONFIG_PLATFORM_COLOR OR CONFIG_PLATFORM_ROUND)
  set(PBL_MIN_SDK_VERSION 3)
else()
  set(PBL_MIN_SDK_VERSION 2)
endif()

if(NOT PBL_BOARD_RUNNERS AND NOT CONFIG_QEMU AND NOT CONFIG_SOC_POSIX)
  message(FATAL_ERROR "Board ${PBL_BOARD} does not define any supported runners")
endif()

# Used for pblboot image naming; -1 when the board has no slots.
if(CONFIG_PBLBOOT)
  set(PBL_SLOT ${CONFIG_FIRMWARE_SLOT})
else()
  set(PBL_SLOT -1)
endif()
