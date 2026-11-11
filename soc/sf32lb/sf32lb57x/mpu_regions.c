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
};
const size_t pbl_soc_num_mpu_regions = ARRAY_LENGTH(pbl_soc_mpu_regions);
