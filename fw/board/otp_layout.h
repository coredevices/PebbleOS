/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/flash.h>

// Factory data in the first OTP region of boards with a SPI NOR flash, 32 bytes per string
#define BOARD_OTP_CELLS                                                                          \
  PBL_FLASH_OTP_CELL("hw-version", 0x00U, 32U), PBL_FLASH_OTP_CELL("serial-number", 0x20U, 32U), \
      PBL_FLASH_OTP_CELL("pcba-serial-number", 0x40U, 32U)

// SoC factory configuration of SF32LB52 boards, on their GD25Q256E (2 KiB regions): the user and
// customer pages, security registers 2 and 3. Its system page, the first 32 bytes of register 1,
// is the hw-version cell.
#define BOARD_OTP_CELLS_SOC_CONFIG                      \
  PBL_FLASH_OTP_CELL("soc-config-user", 0x0800U, 256U), \
      PBL_FLASH_OTP_CELL("soc-config-cust", 0x1000U, 256U)
