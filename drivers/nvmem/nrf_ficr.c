/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/nvmem/nrf_ficr.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

// The registers are read a word at a time
static int prv_reg_read(const struct pbl_nvmem *nvmem, unsigned int offset, void *val,
                        size_t bytes) {
  const struct pbl_nvmem_nrf_ficr *ficr =
      PBL_CONTAINER_OF(nvmem, const struct pbl_nvmem_nrf_ficr, nvmem);
  uint8_t *out = val;

  for (size_t i = 0U; i < bytes; i += sizeof(uint32_t)) {
    const uint32_t word = pbl_readl(ficr->base + offset + i);

    memcpy(&out[i], &word, PBL_MIN(sizeof(word), bytes - i));
  }

  return 0;
}

const struct pbl_nvmem_ops pbl_nvmem_nrf_ficr_ops = {
  .reg_read = prv_reg_read,
};

#include PBL_DT_INSTANCES(nordic_nrf54l15_ficr)
