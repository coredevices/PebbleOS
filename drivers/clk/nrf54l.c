/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/nrf54l.h>
#include <pbl/drivers/nvmem.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/nrf54l_clock.h>
#include <hardware/nrf54l_ficr.h>
#include <hardware/nrf54l_oscillators.h>

#ifdef CONFIG_MPSL
#include <mpsl_pebble.h>
#endif

// The LFXO takes about 250 ms to start, worst case 600 ms
#define LFXO_TIMEOUT_US 1000000U

#define XO24M_RATE_MHZ 24000000000ULL

static const struct pbl_clk_nrf54l *prv_clock(const struct pbl_clk_controller *ctrl) {
  return PBL_CONTAINER_OF(ctrl, const struct pbl_clk_nrf54l, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_clk_nrf54l *clock, uint32_t offset) {
  return pbl_readl(clock->base + offset);
}

static inline void prv_putreg(const struct pbl_clk_nrf54l *clock, uint32_t offset, uint32_t value) {
  pbl_writel(value, clock->base + offset);
}

[[maybe_unused]] static void prv_start(const struct pbl_clk_nrf54l *clock, uint32_t task,
                                       uint32_t event) {
  prv_putreg(clock, event, 0U);
  prv_putreg(clock, task, 1U);
  while (prv_getreg(clock, event) == 0U) {
  }
  prv_putreg(clock, event, 0U);
}

// Slope and offset of the capacitor trim of a crystal, the slope in two's complement
static int prv_trim(const struct pbl_nvmem_cell *cell, int32_t *slope, int32_t *trim_offset) {
  uint32_t trim;
  uint32_t raw;
  uint32_t sign;
  int ret;

  ret = pbl_nvmem_cell_read(cell, &trim, sizeof(trim));
  if (ret != 0) {
    return ret;
  }

  raw = PBL_FIELD_GET(FICR_XOSCTRIM_SLOPE_MASK, trim);
  sign = (FICR_XOSCTRIM_SLOPE_MASK + 1U) >> 1;
  *slope = (int32_t)(raw ^ sign) - (int32_t)sign;
  *trim_offset = (int32_t)PBL_FIELD_GET(FICR_XOSCTRIM_OFFSET_MASK, trim);

  return 0;
}

// INTCAP = ((C - 5.5) * (SLOPE + 791) + OFFSET * 4) / 256, C in pF, rounded
static int prv_hfxo_intcap(const struct pbl_clk_nrf54l *clock, uint32_t *intcap) {
  int32_t slope;
  int32_t offset;
  int32_t mid;
  int ret;

  ret = prv_trim(&clock->hfxo_trim, &slope, &offset);
  if (ret != 0) {
    return ret;
  }

  mid = (((int32_t)clock->hfxo_cap_ff - 5500) * (slope + 791) + offset * 4 * 1000) / 256;
  *intcap = (uint32_t)((mid + 500) / 1000);

  return 0;
}

// INTCAP = (2 * C - 12) * (SLOPE + 392) / 512 + OFFSET / 64, C in pF, rounded
static int prv_lfxo_intcap(const struct pbl_clk_nrf54l *clock, uint32_t *intcap) {
  int32_t slope;
  int32_t offset;
  int32_t mid;
  int ret;

  ret = prv_trim(&clock->lfxo_trim, &slope, &offset);
  if (ret != 0) {
    return ret;
  }

  mid = (2 * (int32_t)clock->lfxo_cap_ff - 12000) * (slope + 392) + offset * 8 * 1000;
  *intcap = (uint32_t)((mid + 256000) / 512000);

  return 0;
}

int pbl_clk_nrf54l_init(const struct pbl_device *dev) {
  const struct pbl_clk_nrf54l *clock =
      prv_clock(PBL_CONTAINER_OF(dev, const struct pbl_clk_controller, dev));
  uint32_t hfxo_intcap = 0U;
  uint32_t lfxo_intcap = 0U;
  int ret;

  ret = pbl_device_init(&clock->hfxo_trim.nvmem->dev);
  if (ret != 0) {
    return ret;
  }

  if (clock->hfxo_cap_ff != 0U) {
    ret = prv_hfxo_intcap(clock, &hfxo_intcap);
    if (ret != 0) {
      return ret;
    }
  }

  if (clock->lfxo_cap_ff != 0U) {
    ret = prv_lfxo_intcap(clock, &lfxo_intcap);
    if (ret != 0) {
      return ret;
    }
  }

  pbl_writel(hfxo_intcap, clock->oscillators + NRF54L_OSCILLATORS_XOSC32M_INTCAP_OFFSET);
  pbl_writel(lfxo_intcap, clock->oscillators + NRF54L_OSCILLATORS_XOSC32KI_INTCAP_OFFSET);

#ifdef CONFIG_MPSL
  return mpsl_pebble_init();
#else
  return 0;
#endif
}

static bool prv_lfclk_running(const struct pbl_clk_nrf54l *clock) {
  const uint32_t stat = prv_getreg(clock, NRF54L_CLOCK_LFCLK_STAT_OFFSET);

  return (stat & CLOCK_LFCLK_STAT_STATE) != 0U &&
         PBL_FIELD_GET(CLOCK_LFCLK_STAT_SRC_MASK, stat) == CLOCK_LFCLK_SRC_LFXO;
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_nrf54l *clock = prv_clock(ctrl);

  // With the MPSL, the HFXO goes through it and the LFXO runs from its start
  switch (id) {
    case NRF54L_CLK_HFCLK:
#ifdef CONFIG_MPSL
      return mpsl_pebble_hfxo(true);
#else
      if ((prv_getreg(clock, NRF54L_CLOCK_XO_STAT_OFFSET) & CLOCK_XO_STAT_STATE) == 0U) {
        prv_start(clock, NRF54L_CLOCK_TASKS_XOSTART_OFFSET, NRF54L_CLOCK_EVENTS_XOSTARTED_OFFSET);
      }
      return 0;
#endif
    case NRF54L_CLK_LFCLK:
#ifdef CONFIG_MPSL
      return PBL_POLL_BUSY_US(prv_lfclk_running(clock), 10U, LFXO_TIMEOUT_US);
#else
      if (!prv_lfclk_running(clock)) {
        prv_putreg(clock, NRF54L_CLOCK_LFCLK_SRC_OFFSET, CLOCK_LFCLK_SRC_LFXO);
        prv_start(clock, NRF54L_CLOCK_TASKS_LFCLKSTART_OFFSET,
                  NRF54L_CLOCK_EVENTS_LFCLKSTARTED_OFFSET);
      }
      return 0;
#endif
#ifdef CONFIG_SOC_NRF54LM20A
    case NRF54L_CLK_XO24M:
      if ((prv_getreg(clock, NRF54L_CLOCK_PLL24M_STAT_OFFSET) & CLOCK_PLL24M_STAT_STATE) == 0U) {
        prv_start(clock, NRF54L_CLOCK_TASKS_XO24MSTART_OFFSET,
                  NRF54L_CLOCK_EVENTS_XO24MSTARTED_OFFSET);
      }
      return 0;
#endif
    default:
      return -EINVAL;
  }
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  [[maybe_unused]] const struct pbl_clk_nrf54l *clock = prv_clock(ctrl);

  switch (id) {
    case NRF54L_CLK_HFCLK:
#ifdef CONFIG_MPSL
      return mpsl_pebble_hfxo(false);
#else
      prv_putreg(clock, NRF54L_CLOCK_TASKS_XOSTOP_OFFSET, 1U);
      return 0;
#endif
    case NRF54L_CLK_LFCLK:
#ifndef CONFIG_MPSL
      prv_putreg(clock, NRF54L_CLOCK_TASKS_LFCLKSTOP_OFFSET, 1U);
#endif
      return 0;
#ifdef CONFIG_SOC_NRF54LM20A
    case NRF54L_CLK_XO24M:
      prv_putreg(clock, NRF54L_CLOCK_TASKS_XO24MSTOP_OFFSET, 1U);
      return 0;
#endif
    default:
      return -EINVAL;
  }
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  switch (id) {
    case NRF54L_CLK_HFCLK:
    case NRF54L_CLK_LFCLK:
      *rate_mhz = parent_mhz;
      return 0;
#ifdef CONFIG_SOC_NRF54LM20A
    case NRF54L_CLK_XO24M:
      *rate_mhz = XO24M_RATE_MHZ;
      return 0;
#endif
    default:
      return -EINVAL;
  }
}

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_nrf54l *clock = prv_clock(ctrl);

  switch (id) {
    case NRF54L_CLK_HFCLK:
      *parent = clock->hfxo;
      return 0;
    case NRF54L_CLK_LFCLK:
      *parent = clock->lfxo;
      return 0;
#ifdef CONFIG_SOC_NRF54LM20A
    case NRF54L_CLK_XO24M:
      *parent = (struct pbl_clk)PBL_CLK(&clock->ctrl, NRF54L_CLK_HFCLK);
      return 0;
#endif
    default:
      return -EINVAL;
  }
}

const struct pbl_clk_controller_ops pbl_clk_nrf54l_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
};

#include PBL_DT_INSTANCES(nordic_nrf54l15_clock)
