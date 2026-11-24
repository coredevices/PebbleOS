/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash/nor.h>

// The secured OTP area is entered with ENSO, not read as security registers
PBL_FLASH_NOR_PART_DEFINE(pbl_flash_nor_mx25r6435f) = {
  .name = "MX25R6435F",
  .id = 0x1728c2U,
  .flags = PBL_FLASH_NOR_F_SECR,
  .geometry =
      {
        .size = 8U * 1024U * 1024U,
        .page_size = 256U,
        .sector_size = 64U * 1024U,
        .subsector_size = 4U * 1024U,
        .sector_erase_ms = 400U,
        .subsector_erase_ms = 40U,
      },
  .qe = PBL_FLASH_NOR_QE_SR1_BIT6,
  .sr1_bp_mask = 0x3CU,
  .power_up_us = 800U,
  .reset_ms = 12U,
  .suspend_us = 60U,
  .resume_us = 100U,
  .dpd_enter_us = 10U,
  .dpd_exit_us = 35U,
};
