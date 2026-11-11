/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk/nrf52.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_clock.h>

#define HFCLK_RATE_MHZ 64000000000ULL

#define HFCLKSTAT_XTAL_RUNNING (CLOCK_HFCLKSTAT_SRC_XTAL | CLOCK_HFCLKSTAT_STATE)
#define LFCLKSTAT_MASK         (CLOCK_LFCLKSTAT_SRC_MASK | CLOCK_LFCLKSTAT_STATE)
#define LFCLKSTAT_XTAL_RUNNING (CLOCK_LFCLKSTAT_SRC_XTAL | CLOCK_LFCLKSTAT_STATE)

struct pbl_clk_nrf52 {
  struct pbl_clk_controller ctrl;
  uintptr_t base;
  struct pbl_clk hfxo;
  struct pbl_clk lfxo;
};

static const struct pbl_clk_nrf52 *prv_clock(const struct pbl_clk_controller *ctrl) {
  return container_of(ctrl, const struct pbl_clk_nrf52, ctrl);
}

static inline uint32_t prv_getreg(const struct pbl_clk_nrf52 *clock, uint32_t offset) {
  return pbl_readl(clock->base + offset);
}

static inline void prv_putreg(const struct pbl_clk_nrf52 *clock, uint32_t offset, uint32_t value) {
  pbl_writel(value, clock->base + offset);
}

static void prv_start(const struct pbl_clk_nrf52 *clock, uint32_t task, uint32_t event) {
  prv_putreg(clock, event, 0U);
  prv_putreg(clock, task, 1U);
  // TODO: time out and fall back to the calibrated RC for LFCLK, which needs a runtime BLE SCA.
  // Until then, the bootloader's watchdog reboots if a crystal does not start.
  while (prv_getreg(clock, event) == 0U) {
  }
  prv_putreg(clock, event, 0U);
}

static int prv_enable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_nrf52 *clock = prv_clock(ctrl);

  switch (id) {
    case NRF52_CLK_HFCLK:
      if ((prv_getreg(clock, NRF52_CLOCK_HFCLKSTAT_OFFSET) & HFCLKSTAT_XTAL_RUNNING) !=
          HFCLKSTAT_XTAL_RUNNING) {
        prv_start(clock, NRF52_CLOCK_TASKS_HFCLKSTART_OFFSET,
                  NRF52_CLOCK_EVENTS_HFCLKSTARTED_OFFSET);
      }
      return 0;
    case NRF52_CLK_LFCLK:
      if ((prv_getreg(clock, NRF52_CLOCK_LFCLKSTAT_OFFSET) & LFCLKSTAT_MASK) !=
          LFCLKSTAT_XTAL_RUNNING) {
        prv_putreg(clock, NRF52_CLOCK_LFCLKSRC_OFFSET, CLOCK_LFCLKSRC_SRC_XTAL);
        prv_start(clock, NRF52_CLOCK_TASKS_LFCLKSTART_OFFSET,
                  NRF52_CLOCK_EVENTS_LFCLKSTARTED_OFFSET);
      }
      return 0;
    default:
      return -EINVAL;
  }
}

static int prv_disable(const struct pbl_clk_controller *ctrl, uint32_t id) {
  const struct pbl_clk_nrf52 *clock = prv_clock(ctrl);

  switch (id) {
    case NRF52_CLK_HFCLK:
      prv_putreg(clock, NRF52_CLOCK_TASKS_HFCLKSTOP_OFFSET, 1U);
      return 0;
    case NRF52_CLK_LFCLK:
      prv_putreg(clock, NRF52_CLOCK_TASKS_LFCLKSTOP_OFFSET, 1U);
      return 0;
    default:
      return -EINVAL;
  }
}

static int prv_get_rate(const struct pbl_clk_controller *ctrl, uint32_t id, uint64_t parent_mhz,
                        uint64_t *rate_mhz) {
  switch (id) {
    case NRF52_CLK_HFCLK:
      *rate_mhz = HFCLK_RATE_MHZ;
      return 0;
    case NRF52_CLK_LFCLK:
      *rate_mhz = parent_mhz;
      return 0;
    default:
      return -EINVAL;
  }
}

static int prv_get_parent(const struct pbl_clk_controller *ctrl, uint32_t id,
                          struct pbl_clk *parent) {
  const struct pbl_clk_nrf52 *clock = prv_clock(ctrl);

  switch (id) {
    case NRF52_CLK_HFCLK:
      *parent = clock->hfxo;
      return 0;
    case NRF52_CLK_LFCLK:
      *parent = clock->lfxo;
      return 0;
    default:
      return -EINVAL;
  }
}

static const struct pbl_clk_controller_ops s_ops = {
  .enable = prv_enable,
  .disable = prv_disable,
  .get_rate = prv_get_rate,
  .get_parent = prv_get_parent,
};

#define PBL_CLK_NRF52_DEFINE(sym, _name, _base, _hfxo, _lfxo)                               \
  PBL_CLK_CONTROLLER_STATE_DEFINE(sym, 2);                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_clk_nrf52, struct pbl_clk_controller, ctrl) {         \
    .ctrl = PBL_CLK_CONTROLLER_INIT(sym, _name, nullptr, nullptr, &s_ops), .base = (_base), \
    .hfxo = _hfxo, .lfxo = _lfxo,                                                           \
  }

#include PBL_DT_INSTANCES(nordic_nrf52832_clock)
