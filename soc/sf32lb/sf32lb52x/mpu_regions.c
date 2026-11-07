/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/mpu.h>
#include <pbl/util/size.h>

extern const uint8_t __ramfunc_start[];
extern const uint8_t __ramfunc_size[];

const struct pbl_mpu_region pbl_soc_mpu_regions[] = {
  {
    .name = "peripherals",
    .base = 0x40000000U,
    .size = 0x20000000U,
    .attr = PBL_MPU_P_RW | PBL_MPU_DEVICE,
  },
  {
    // Copied at boot, before the kernel takes over the MPU.
    .name = "ramfunc",
    .base = (uintptr_t)__ramfunc_start,
    .size = (size_t)__ramfunc_size,
    .attr = PBL_MPU_P_RO | PBL_MPU_NORMAL_NC | PBL_MPU_EXEC,
  },
  {
    .name = "lpsys ram",
    .base = 0x203fc000U,
    .size = 0x00104000U,
    .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_NC | PBL_MPU_EXEC,
  },
  {
    // The LCPU reads and writes the last 1 KiB of HPSYS SRAM directly.
    .name = "lcpu mailbox",
    .base = 0x2007fc00U,
    .size = 0x00000400U,
    .attr = PBL_MPU_P_RW | PBL_MPU_NORMAL_NC,
  },
};
const size_t pbl_soc_num_mpu_regions = ARRAY_LENGTH(pbl_soc_mpu_regions);
