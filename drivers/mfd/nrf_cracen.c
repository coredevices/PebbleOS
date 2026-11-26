/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/devicetree.h>
#include <pbl/drivers/mfd/nrf_cracen.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf54l_cracen.h>

void pbl_nrf_cracen_enable(const struct pbl_nrf_cracen *cracen, uint32_t parts) {
  const uintptr_t reg = cracen->wrapper + NRF54L_CRACEN_ENABLE_OFFSET;

  pbl_irq_lock();
  pbl_writel(pbl_readl(reg) | parts, reg);
  pbl_irq_unlock();
}

void pbl_nrf_cracen_disable(const struct pbl_nrf_cracen *cracen, uint32_t parts) {
  const uintptr_t reg = cracen->wrapper + NRF54L_CRACEN_ENABLE_OFFSET;

  pbl_irq_lock();
  pbl_writel(pbl_readl(reg) & ~parts, reg);
  pbl_irq_unlock();
}

int pbl_nrf_cracen_init(const struct pbl_device *dev) {
  const struct pbl_nrf_cracen *cracen = PBL_CONTAINER_OF(dev, const struct pbl_nrf_cracen, dev);

  pbl_writel(0U, cracen->wrapper + NRF54L_CRACEN_ENABLE_OFFSET);

  return 0;
}

#include PBL_DT_INSTANCES(nordic_nrf54l15_cracen)
