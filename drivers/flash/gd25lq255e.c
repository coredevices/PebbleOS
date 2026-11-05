/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash/nor.h>

const struct pbl_flash_nor_part pbl_flash_nor_gd25lq255e = {
  .name = "GD25LQ255E",
  .id = 0x1960c8U,
  .flags = PBL_FLASH_NOR_F_4B_OPCODES,
  .geometry =
      {
        .size = 32U * 1024U * 1024U,
        .page_size = 256U,
        .sector_size = 64U * 1024U,
        .subsector_size = 4U * 1024U,
        .sector_erase_ms = 150U,
        .subsector_erase_ms = 50U,
      },
  // Security registers 2 and 3 only, locked by LB2 and LB3 (status register 2 bits 4 and 5)
  .otp = PBL_FLASH_NOR_OTP(1024U, 2U, 0x2000U, 0x1000U, 4U),
  .qe = PBL_FLASH_NOR_QE_SR2_BIT1_WR_SR1,
  .sr1_bp_mask = 0x7CU,
  .reset_ms = 12U,
  .suspend_us = 20U,
  .resume_us = 100U,
  .dpd_enter_us = 3U,
  .dpd_exit_us = 20U,
};
