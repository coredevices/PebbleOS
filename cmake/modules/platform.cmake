# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Board-level facts derived from the configuration.

if(NOT PBL_BOARD_RUNNERS AND NOT CONFIG_QEMU AND NOT CONFIG_SOC_POSIX)
  message(FATAL_ERROR "Board ${PBL_BOARD} does not define any supported runners")
endif()

# Used for pblboot image naming; -1 when the board has no slots.
if(CONFIG_PBLBOOT)
  set(PBL_SLOT ${CONFIG_FIRMWARE_SLOT})
else()
  set(PBL_SLOT -1)
endif()
