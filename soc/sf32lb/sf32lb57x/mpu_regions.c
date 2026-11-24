/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/mpu.h>
#include <pbl/util/size.h>

#include <devicetree/regs.h>

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
    // The LCPU and ACPU read and write their mailbox buffers directly.
    .name = "lcpu mailbox",
    .base = PBL_DT_REG_IPC_BUFS,
    .size = PBL_DT_REG_LCPU_IPC + PBL_DT_SIZE_LCPU_IPC - PBL_DT_REG_IPC_BUFS,
    .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_NC,
  },
};
const size_t pbl_soc_num_mpu_regions = PBL_ARRAY_SIZE(pbl_soc_mpu_regions);
