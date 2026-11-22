/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/nvmem/stm32_romem.h>
#include <pbl/util/misc.h>

// The memory is read as any other, only never written
static int prv_reg_read(const struct pbl_nvmem *nvmem, unsigned int offset, void *val,
                        size_t bytes) {
  const struct pbl_nvmem_stm32_romem *romem =
      PBL_CONTAINER_OF(nvmem, const struct pbl_nvmem_stm32_romem, nvmem);

  memcpy(val, (const void *)(romem->base + offset), bytes);

  return 0;
}

const struct pbl_nvmem_ops pbl_nvmem_stm32_romem_ops = {
  .reg_read = prv_reg_read,
};

#include PBL_DT_INSTANCES(st_stm32f4_otp)
