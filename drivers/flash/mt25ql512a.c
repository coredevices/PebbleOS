/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash/nor.h>

// The OTP array is not a set of security registers
PBL_FLASH_NOR_PART_DEFINE(pbl_flash_nor_mt25ql512a) = {
  .name = "MT25QL512A",
  .id = 0x20ba20U,
  .flags = PBL_FLASH_NOR_F_4B_OPCODES | PBL_FLASH_NOR_F_FSR,
  .geometry =
      {
        .size = 64U * 1024U * 1024U,
        .page_size = 256U,
        .sector_size = 64U * 1024U,
        .subsector_size = 4U * 1024U,
        .sector_erase_ms = 150U,
        .subsector_erase_ms = 50U,
      },
  .qe = PBL_FLASH_NOR_QE_NONE,
  .sr1_bp_mask = 0x7CU,
  .power_up_us = 1000U,
  .reset_ms = 1U,
  .suspend_us = 30U,
  .resume_us = 100U,
  .dpd_enter_us = 3U,
  .dpd_exit_us = 30U,
};
