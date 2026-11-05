/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/nrf/ppi.h>
#include <pbl/util/bits.h>

#include <hardware/nrf52_ppi.h>

#ifdef CONFIG_NIMBLE_DRIVER_NRF5X
// Programmed directly by the NimBLE radio driver
#define RESERVED_CHANNELS (PBL_GENMASK(7, 4) | PBL_GENMASK(19, 17))
#else
#define RESERVED_CHANNELS 0U
#endif

static uint32_t s_allocated = RESERVED_CHANNELS;

int soc_nrf_ppi_alloc(uint8_t *channel) {
  int ret = -ENOMEM;

  pbl_irq_lock();

  for (uint8_t ch = 0U; ch < NRF52_PPI_NUM_CHANNELS; ch++) {
    if ((s_allocated & PBL_BIT(ch)) == 0U) {
      s_allocated |= PBL_BIT(ch);
      *channel = ch;
      ret = 0;
      break;
    }
  }

  pbl_irq_unlock();

  return ret;
}

void soc_nrf_ppi_setup(uint8_t channel, uintptr_t eep, uintptr_t tep, uintptr_t fork_tep) {
  pbl_writel(eep, NRF52_PPI_BASE + NRF52_PPI_CH_EEP_OFFSET(channel));
  pbl_writel(tep, NRF52_PPI_BASE + NRF52_PPI_CH_TEP_OFFSET(channel));
  pbl_writel(fork_tep, NRF52_PPI_BASE + NRF52_PPI_FORK_TEP_OFFSET(channel));
}

void soc_nrf_ppi_enable(uint32_t mask) {
  pbl_writel(mask, NRF52_PPI_BASE + NRF52_PPI_CHENSET_OFFSET);
}

void soc_nrf_ppi_disable(uint32_t mask) {
  pbl_writel(mask, NRF52_PPI_BASE + NRF52_PPI_CHENCLR_OFFSET);
}
