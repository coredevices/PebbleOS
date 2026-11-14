/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/mpu.h>
#include <pbl/util/size.h>

const struct pbl_mpu_region pbl_soc_mpu_regions[] = {
  {
    .name = "peripherals",
    .base = 0x40000000U,
    .size = 0x20000000U,
    .attr = PBL_MPU_P_RW | PBL_MPU_DEVICE,
  },
  {
    // Shared with the LCPU, which writes it directly
    .name = "lpsys ram",
    .base = 0x20400000U,
    .size = 0x00014000U,
    .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_NC,
  },
  {
    // The LCPU reads and writes the last 2 KiB of HPSYS SRAM directly.
    .name = "lcpu mailbox",
    .base = 0x2007f800U,
    .size = 0x00000800U,
    .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_NC,
  },
};
const size_t pbl_soc_num_mpu_regions = ARRAY_LENGTH(pbl_soc_mpu_regions);
