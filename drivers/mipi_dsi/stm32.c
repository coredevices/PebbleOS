/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/display_bridge.h>
#include <pbl/drivers/mipi_dsi/stm32.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/units.h>

#include <hardware/stm32_dsi.h>

// The D-PHY runs up to 500 Mbit/s per lane, and its PLL VCO between 500 MHz and 1 GHz
#define LANE_BIT_RATE_HZ 500000000U
#define VCO_MIN_HZ       500000000U
#define VCO_MAX_HZ       1000000000U
#define PLL_IDF_MAX      7U
#define PLL_NDIV_MIN     10U
#define PLL_NDIV_MAX     125U

// The escape clock runs up to 20 MHz
#define ESC_CLK_MAX_HZ 20000000U

// Lane transition times, in lane byte clock cycles, as ST gives them for the link rate
#define CLK_HS2LP_TIME  28U
#define CLK_LP2HS_TIME  33U
#define DATA_HS2LP_TIME 15U
#define DATA_LP2HS_TIME 25U

#define REG_TIMEOUT_US 10000U
#define PLL_TIMEOUT_US 10000U
#define CMD_TIMEOUT_US 100000U

static const struct pbl_mipi_dsi_stm32 *prv_dsi(const struct pbl_mipi_dsi_host *host) {
  return PBL_CONTAINER_OF(host, const struct pbl_mipi_dsi_stm32, host);
}

static bool prv_te_pin(const struct pbl_mipi_dsi_stm32 *dsi) {
  return dsi->pinctrl.states[PBL_PINCTRL_STATE_DEFAULT] != nullptr;
}

static inline uint32_t prv_getreg(const struct pbl_mipi_dsi_stm32 *dsi, uint32_t offset) {
  return pbl_readl(dsi->base + offset);
}

static inline void prv_putreg(const struct pbl_mipi_dsi_stm32 *dsi, uint32_t offset,
                              uint32_t value) {
  pbl_writel(value, dsi->base + offset);
}

static int prv_wait(const struct pbl_mipi_dsi_stm32 *dsi, uint32_t offset, uint32_t mask,
                    uint32_t value, uint32_t timeout_us) {
  return PBL_POLL_BUSY_US((prv_getreg(dsi, offset) & mask) == value, 1U, timeout_us);
}

static int prv_rate_hz(const struct pbl_clk *clk, uint32_t *hz) {
  uint64_t rate_mhz;
  int ret = pbl_clk_get_rate(clk, &rate_mhz);

  if (ret != 0) {
    return ret;
  }

  *hz = (uint32_t)(rate_mhz / PBL_MILLIHZ_PER_HZ);

  return *hz != 0U ? 0 : -EINVAL;
}

// The PLL gives ref / IDF * NDIV / ODF, with its VCO at twice ref / IDF * NDIV
static int prv_pll_start(const struct pbl_mipi_dsi_stm32 *dsi, uint32_t ref_hz) {
  static const uint8_t s_odfs[] = {1U, 2U, 4U, 8U};

  for (uint32_t idf = PLL_IDF_MAX; idf >= 1U; idf--) {
    for (uint32_t o = 0U; o < sizeof(s_odfs); o++) {
      uint64_t num = (uint64_t)LANE_BIT_RATE_HZ * idf * s_odfs[o];
      uint64_t vco;
      uint32_t ndiv;

      if (num % ref_hz != 0U) {
        continue;
      }
      ndiv = (uint32_t)(num / ref_hz);
      vco = 2ULL * ref_hz * ndiv / idf;
      if (ndiv < PLL_NDIV_MIN || ndiv > PLL_NDIV_MAX || vco < VCO_MIN_HZ || vco > VCO_MAX_HZ) {
        continue;
      }

      prv_putreg(dsi, STM32_DSI_WRPCR_OFFSET,
                 DSI_WRPCR_REGEN | PBL_FIELD_PREP(DSI_WRPCR_PLL_NDIV_MASK, ndiv) |
                     PBL_FIELD_PREP(DSI_WRPCR_PLL_IDF_MASK, idf) |
                     PBL_FIELD_PREP(DSI_WRPCR_PLL_ODF_MASK, o));
      pbl_modifyl(0U, DSI_WRPCR_PLLEN, dsi->base + STM32_DSI_WRPCR_OFFSET);

      return prv_wait(dsi, STM32_DSI_WISR_OFFSET, DSI_WISR_PLLLS, DSI_WISR_PLLLS, PLL_TIMEOUT_US);
    }
  }

  return -ENOTSUP;
}

static uint32_t prv_color(enum pbl_mipi_dsi_pixel_format format) {
  switch (format) {
    case PBL_MIPI_DSI_FMT_RGB565:
      return DSI_COLOR_16BIT;
    case PBL_MIPI_DSI_FMT_RGB666_PACKED:
      return DSI_COLOR_18BIT;
    default:
      return DSI_COLOR_24BIT;
  }
}

// Adapted command mode: the wrapper sends a frame of the display controller on each refresh,
// starting on the rising edge of its vertical sync. The panel reports its tearing effect on the
// TE pin, when the host has one, else over the link
static void prv_command_mode(const struct pbl_mipi_dsi_stm32 *dsi,
                             const struct pbl_mipi_dsi_device *dev) {
  uint32_t color = prv_color(dev->format);

  pbl_modifyl(0U, DSI_MCR_CMDM, dsi->base + STM32_DSI_MCR_OFFSET);
  prv_putreg(dsi, STM32_DSI_WCFGR_OFFSET,
             DSI_WCFGR_DSIM | PBL_FIELD_PREP(DSI_WCFGR_COLMUX_MASK, color) | DSI_WCFGR_VSPOL |
                 (prv_te_pin(dsi) ? DSI_WCFGR_TESRC : 0U));
  prv_putreg(dsi, STM32_DSI_LCOLCR_OFFSET, PBL_FIELD_PREP(DSI_LCOLCR_COLC_MASK, color));
  prv_putreg(dsi, STM32_DSI_LPCR_OFFSET, 0U);
  prv_putreg(dsi, STM32_DSI_LVCIDR_OFFSET, dev->channel);
  prv_putreg(dsi, STM32_DSI_LCCR_OFFSET,
             PBL_FIELD_PREP(DSI_LCCR_CMDSIZE_MASK, dev->timing->hactive));
  pbl_modifyl(0U, DSI_CMCR_TEARE, dsi->base + STM32_DSI_CMCR_OFFSET);
  pbl_modifyl(0U, DSI_WPCR1_FLPRXLPM, dsi->base + STM32_DSI_WPCR1_OFFSET);
  prv_putreg(dsi, STM32_DSI_WIFCR_OFFSET, DSI_WIRQ_TE | DSI_WIRQ_ER);
  prv_putreg(dsi, STM32_DSI_WIER_OFFSET, DSI_WIRQ_ER);
}

// Only adapted command mode is set up, as ST sets the panels of its boards up; loosely packed
// 18-bit pixels are not
static int prv_attach(const struct pbl_mipi_dsi_host *host, const struct pbl_mipi_dsi_device *dev) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_dsi(host);
  const uint32_t lbc_hz = LANE_BIT_RATE_HZ / 8U;
  uint32_t ref_hz;
  int ret;

  if ((dev->mode_flags & PBL_MIPI_DSI_MODE_VIDEO) != 0U || dev->lanes > 2U ||
      dev->timing == nullptr || dev->format == PBL_MIPI_DSI_FMT_RGB666) {
    return -ENOTSUP;
  }

  ret = prv_rate_hz(&dsi->ref, &ref_hz);
  if (ret != 0) {
    return ret;
  }

  if (prv_te_pin(dsi)) {
    ret = pbl_pinctrl_select_state(&dsi->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
    if (ret != 0) {
      return ret;
    }
  }

  pbl_modifyl(0U, DSI_WRPCR_REGEN, dsi->base + STM32_DSI_WRPCR_OFFSET);
  ret = prv_wait(dsi, STM32_DSI_WISR_OFFSET, DSI_WISR_RRS, DSI_WISR_RRS, REG_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  ret = prv_pll_start(dsi, ref_hz);
  if (ret != 0) {
    return ret;
  }

  // The unit interval, in quarters of a nanosecond
  prv_putreg(dsi, STM32_DSI_WPCR0_OFFSET,
             PBL_FIELD_PREP(DSI_WPCR0_UIX4_MASK, 4000000000ULL / LANE_BIT_RATE_HZ));

  prv_putreg(dsi, STM32_DSI_PCTLR_OFFSET, DSI_PCTLR_CKE | DSI_PCTLR_DEN);
  prv_putreg(dsi, STM32_DSI_CLCR_OFFSET, DSI_CLCR_DPCC);
  prv_putreg(dsi, STM32_DSI_PCONFR_OFFSET, PBL_FIELD_PREP(DSI_PCONFR_NL_MASK, dev->lanes - 1U));
  prv_putreg(
      dsi, STM32_DSI_CCR_OFFSET,
      PBL_FIELD_PREP(DSI_CCR_TXECKDIV_MASK, (lbc_hz + ESC_CLK_MAX_HZ - 1U) / ESC_CLK_MAX_HZ));
  prv_putreg(dsi, STM32_DSI_CLTCR_OFFSET,
             PBL_FIELD_PREP(DSI_CLTCR_LP2HS_TIME_MASK, CLK_LP2HS_TIME) |
                 PBL_FIELD_PREP(DSI_CLTCR_HS2LP_TIME_MASK, CLK_HS2LP_TIME));
  prv_putreg(dsi, STM32_DSI_DLTCR_OFFSET,
             PBL_FIELD_PREP(DSI_DLTCR_LP2HS_TIME_MASK, DATA_LP2HS_TIME) |
                 PBL_FIELD_PREP(DSI_DLTCR_HS2LP_TIME_MASK, DATA_HS2LP_TIME));
  prv_putreg(dsi, STM32_DSI_PCR_OFFSET, DSI_PCR_BTAE);

  prv_command_mode(dsi, dev);

  pbl_modifyl(0U, DSI_CR_EN, dsi->base + STM32_DSI_CR_OFFSET);
  pbl_modifyl(0U, DSI_WCR_DSIEN, dsi->base + STM32_DSI_WCR_OFFSET);

  return 0;
}

static int prv_detach(const struct pbl_mipi_dsi_host *host, const struct pbl_mipi_dsi_device *dev) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_dsi(host);

  prv_putreg(dsi, STM32_DSI_WIER_OFFSET, 0U);
  pbl_modifyl(DSI_WCR_DSIEN, 0U, dsi->base + STM32_DSI_WCR_OFFSET);
  pbl_modifyl(DSI_CR_EN, 0U, dsi->base + STM32_DSI_CR_OFFSET);
  prv_putreg(dsi, STM32_DSI_PCTLR_OFFSET, 0U);
  prv_putreg(dsi, STM32_DSI_WRPCR_OFFSET, 0U);

  return 0;
}

static bool prv_is_long(uint8_t type) {
  return type == PBL_MIPI_DSI_GENERIC_LONG_WRITE || type == PBL_MIPI_DSI_DCS_LONG_WRITE;
}

static ssize_t prv_read(const struct pbl_mipi_dsi_stm32 *dsi, uint8_t *buf, size_t len) {
  size_t n = 0U;

  while (n < len) {
    uint32_t word;
    int ret;

    // A shorter response than asked for ends with the FIFO empty
    ret = prv_wait(dsi, STM32_DSI_GPSR_OFFSET, DSI_GPSR_PRDFE, 0U, CMD_TIMEOUT_US);
    if (ret != 0) {
      return n > 0U ? (ssize_t)n : ret;
    }

    word = prv_getreg(dsi, STM32_DSI_GPDR_OFFSET);
    for (uint32_t i = 0U; i < 4U && n < len; i++, n++) {
      buf[n] = (uint8_t)(word >> (8U * i));
    }
  }

  return (ssize_t)n;
}

static ssize_t prv_transfer(const struct pbl_mipi_dsi_host *host,
                            const struct pbl_mipi_dsi_msg *msg) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_dsi(host);
  uint32_t ghcr;
  int ret;

  if (!prv_is_long(msg->type) && msg->tx_len > 2U) {
    return -EINVAL;
  }

  ret = prv_wait(dsi, STM32_DSI_GPSR_OFFSET, DSI_GPSR_CMDFE, DSI_GPSR_CMDFE, CMD_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(DSI_CMCR_LP_ALL, msg->lpm ? DSI_CMCR_LP_ALL : 0U, dsi->base + STM32_DSI_CMCR_OFFSET);

  ghcr = PBL_FIELD_PREP(DSI_GHCR_DT_MASK, msg->type) |
         PBL_FIELD_PREP(DSI_GHCR_VCID_MASK, msg->channel);

  if (prv_is_long(msg->type)) {
    // The payload goes first, in little-endian words, then the header with its length
    for (size_t i = 0U; i < msg->tx_len; i += 4U) {
      uint32_t word = 0U;

      for (size_t j = 0U; j < 4U && i + j < msg->tx_len; j++) {
        word |= (uint32_t)msg->tx_buf[i + j] << (8U * j);
      }
      ret = prv_wait(dsi, STM32_DSI_GPSR_OFFSET, DSI_GPSR_PWRFF, 0U, CMD_TIMEOUT_US);
      if (ret != 0) {
        return ret;
      }
      prv_putreg(dsi, STM32_DSI_GPDR_OFFSET, word);
    }
    ghcr |= PBL_FIELD_PREP(DSI_GHCR_WCLSB_MASK, msg->tx_len & 0xffU) |
            PBL_FIELD_PREP(DSI_GHCR_WCMSB_MASK, (msg->tx_len >> 8) & 0xffU);
  } else {
    if (msg->tx_len > 0U) {
      ghcr |= PBL_FIELD_PREP(DSI_GHCR_WCLSB_MASK, msg->tx_buf[0]);
    }
    if (msg->tx_len > 1U) {
      ghcr |= PBL_FIELD_PREP(DSI_GHCR_WCMSB_MASK, msg->tx_buf[1]);
    }
  }

  prv_putreg(dsi, STM32_DSI_GHCR_OFFSET, ghcr);

  if (msg->rx_buf != nullptr && msg->rx_len > 0U) {
    return prv_read(dsi, msg->rx_buf, msg->rx_len);
  }

  return (ssize_t)msg->tx_len;
}

static const struct pbl_mipi_dsi_stm32 *prv_bridge_dsi(const struct pbl_display_bridge *bridge) {
  return PBL_CONTAINER_OF(bridge, const struct pbl_mipi_dsi_stm32_bridge, bridge)->dsi;
}

// The commands before a refresh, as the window of the frame, must be out before its pixels
static int prv_refresh(const struct pbl_display_bridge *bridge) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_bridge_dsi(bridge);
  int ret;

  ret = prv_wait(dsi, STM32_DSI_GPSR_OFFSET, DSI_GPSR_CMDFE, DSI_GPSR_CMDFE, CMD_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(0U, DSI_WCR_LTDCEN, dsi->base + STM32_DSI_WCR_OFFSET);

  return 0;
}

// The wrapper lets the LTDC be set up while it is disabled
static void prv_hold(const struct pbl_display_bridge *bridge, bool hold) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_bridge_dsi(bridge);

  pbl_modifyl(hold ? DSI_WCR_DSIEN : 0U, hold ? 0U : DSI_WCR_DSIEN,
              dsi->base + STM32_DSI_WCR_OFFSET);
}

// The panel reports its tearing effect on every frame: the next one is let through, once
static int prv_request_tear(const struct pbl_display_bridge *bridge) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_bridge_dsi(bridge);

  pbl_irq_lock();
  prv_putreg(dsi, STM32_DSI_WIFCR_OFFSET, DSI_WIRQ_TE);
  pbl_modifyl(0U, DSI_WIRQ_TE, dsi->base + STM32_DSI_WIER_OFFSET);
  pbl_irq_unlock();

  return 0;
}

static void prv_set_event_cb(const struct pbl_display_bridge *bridge,
                             pbl_display_bridge_event_cb_t cb, void *arg) {
  struct pbl_mipi_dsi_stm32_state *st = prv_bridge_dsi(bridge)->state;

  pbl_irq_lock();
  st->cb = cb;
  st->arg = arg;
  pbl_irq_unlock();
}

const struct pbl_display_bridge_ops pbl_mipi_dsi_stm32_bridge_ops = {
  .refresh = prv_refresh,
  .hold = prv_hold,
  .request_tear = prv_request_tear,
  .set_event_cb = prv_set_event_cb,
};

void pbl_mipi_dsi_stm32_irq_handler(const struct pbl_mipi_dsi_stm32 *dsi) {
  struct pbl_mipi_dsi_stm32_state *st = dsi->state;
  uint32_t wisr = prv_getreg(dsi, STM32_DSI_WISR_OFFSET) & prv_getreg(dsi, STM32_DSI_WIER_OFFSET);

  prv_putreg(dsi, STM32_DSI_WIFCR_OFFSET, wisr);
  if ((wisr & DSI_WIRQ_TE) != 0U) {
    pbl_modifyl(DSI_WIRQ_TE, 0U, dsi->base + STM32_DSI_WIER_OFFSET);
  }

  if (st->cb == nullptr) {
    return;
  }

  if ((wisr & DSI_WIRQ_TE) != 0U) {
    st->cb(PBL_DISPLAY_BRIDGE_EVENT_TEAR, st->arg);
  }

  if ((wisr & DSI_WIRQ_ER) != 0U) {
    st->cb(PBL_DISPLAY_BRIDGE_EVENT_REFRESH_DONE, st->arg);
  }
}

int pbl_mipi_dsi_stm32_init(const struct pbl_device *dev) {
  const struct pbl_mipi_dsi_stm32 *dsi =
      PBL_CONTAINER_OF(dev, const struct pbl_mipi_dsi_stm32, host.dev);
  int ret;

  ret = pbl_device_init(&dsi->pclk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&dsi->pclk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&dsi->ref);
  if (ret != 0) {
    return ret;
  }

  pbl_irq_enable(dsi->irq);

  return 0;
}

const struct pbl_mipi_dsi_host_ops pbl_mipi_dsi_stm32_ops = {
  .attach = prv_attach,
  .detach = prv_detach,
  .transfer = prv_transfer,
};

#include PBL_DT_INSTANCES(st_stm32_dsi)
