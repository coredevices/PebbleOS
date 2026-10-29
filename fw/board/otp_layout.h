/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash.h>

// Factory data in the first OTP region of boards with a SPI NOR flash, 32 bytes per string
#define BOARD_OTP_CELLS                                                                          \
  PBL_FLASH_OTP_CELL("hw-version", 0x00U, 32U), PBL_FLASH_OTP_CELL("serial-number", 0x20U, 32U), \
      PBL_FLASH_OTP_CELL("pcba-serial-number", 0x40U, 32U)
