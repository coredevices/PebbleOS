# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Board- and platform-level facts derived from the configuration: the SDK
# platform the firmware exposes.

if(CONFIG_PLATFORM_EMERY)
  set(PBL_PLATFORM_NAME emery)
  set(PBL_MIN_SDK_VERSION 3)
elseif(CONFIG_PLATFORM_FLINT)
  set(PBL_PLATFORM_NAME flint)
  set(PBL_MIN_SDK_VERSION 2)
elseif(CONFIG_PLATFORM_GABBRO)
  set(PBL_PLATFORM_NAME gabbro)
  set(PBL_MIN_SDK_VERSION 3)
else()
  message(FATAL_ERROR "No platform specified for ${PBL_BOARD}")
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
