/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/mipi_dsi/stm32.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>

#include <hardware/stm32_dsi.h>

#define MHZ_PER_HZ 1000ULL

// The D-PHY runs up to 500 Mbit/s per lane, and its PLL VCO between 500 MHz and 1 GHz
#define LANE_BIT_RATE_HZ 500000000U
#define VCO_MIN_HZ       500000000U
#define VCO_MAX_HZ       1000000000U
#define PLL_IDF_MAX      7U
#define PLL_NDIV_MIN     10U
#define PLL_NDIV_MAX     125U

// The escape clock runs up to 20 MHz
#define ESC_CLK_MAX_HZ 20000000U

// Lane transition times, in lane byte clock cycles, and the stop state wait
#define LANE_SWITCH_TIME 35U
#define STOP_WAIT_TIME   10U

// Largest packets in the low-power parts of the video lines, in bytes
#define LP_PACKET_SIZE 16U

#define REG_TIMEOUT_US 10000U
#define PLL_TIMEOUT_US 10000U
// In video mode, low-power commands only go out in the vertical blanking, once a frame
#define CMD_TIMEOUT_US 100000U

static const struct pbl_mipi_dsi_stm32 *prv_dsi(const struct pbl_mipi_dsi_host *host) {
  return PBL_CONTAINER_OF(host, const struct pbl_mipi_dsi_stm32, host);
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

  *hz = (uint32_t)(rate_mhz / MHZ_PER_HZ);

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
    case PBL_MIPI_DSI_FMT_RGB666:
    case PBL_MIPI_DSI_FMT_RGB666_PACKED:
      return DSI_COLOR_18BIT;
    default:
      return DSI_COLOR_24BIT;
  }
}

static uint32_t prv_bpp(enum pbl_mipi_dsi_pixel_format format) {
  switch (format) {
    case PBL_MIPI_DSI_FMT_RGB565:
      return 16U;
    case PBL_MIPI_DSI_FMT_RGB666_PACKED:
      return 18U;
    default:
      return 24U;
  }
}

// Pixel clocks to lane byte clocks, the unit of the horizontal timing
static uint32_t prv_lbc(uint32_t pixels, uint32_t lbc_hz, uint32_t px_hz) {
  return (uint32_t)((uint64_t)pixels * lbc_hz / px_hz);
}

static void prv_video_mode(const struct pbl_mipi_dsi_stm32 *dsi,
                           const struct pbl_mipi_dsi_device *dev, uint32_t lbc_hz, uint32_t px_hz) {
  const struct pbl_panel_timing *t = dev->timing;
  uint32_t color = prv_color(dev->format);
  uint32_t vmt = DSI_VMCR_VMT_SYNC_EVENTS;

  if ((dev->mode_flags & PBL_MIPI_DSI_MODE_VIDEO_BURST) != 0U) {
    vmt = DSI_VMCR_VMT_BURST;
  } else if ((dev->mode_flags & PBL_MIPI_DSI_MODE_VIDEO_SYNC_PULSE) != 0U) {
    vmt = DSI_VMCR_VMT_SYNC_PULSES;
  }

  pbl_modifyl(DSI_MCR_CMDM, 0U, dsi->base + STM32_DSI_MCR_OFFSET);
  pbl_modifyl(DSI_WCFGR_DSIM | DSI_WCFGR_COLMUX_MASK, PBL_FIELD_PREP(DSI_WCFGR_COLMUX_MASK, color),
              dsi->base + STM32_DSI_WCFGR_OFFSET);
  prv_putreg(dsi, STM32_DSI_LCOLCR_OFFSET, PBL_FIELD_PREP(DSI_LCOLCR_COLC_MASK, color));
  prv_putreg(dsi, STM32_DSI_LPCR_OFFSET, 0U);
  prv_putreg(dsi, STM32_DSI_LVCIDR_OFFSET, dev->channel);

  prv_putreg(dsi, STM32_DSI_VMCR_OFFSET,
             PBL_FIELD_PREP(DSI_VMCR_VMT_MASK, vmt) | DSI_VMCR_LP_ALL | DSI_VMCR_LPCE);
  prv_putreg(dsi, STM32_DSI_LPMCR_OFFSET, PBL_FIELD_PREP(DSI_LPMCR_LPSIZE_MASK, LP_PACKET_SIZE));
  prv_putreg(dsi, STM32_DSI_VPCR_OFFSET, PBL_FIELD_PREP(DSI_VPCR_VPSIZE_MASK, t->hactive));
  prv_putreg(dsi, STM32_DSI_VCCR_OFFSET, 0U);
  prv_putreg(dsi, STM32_DSI_VNPCR_OFFSET, DSI_VNPCR_NPSIZE_MASK);

  prv_putreg(dsi, STM32_DSI_VHSACR_OFFSET,
             PBL_FIELD_PREP(DSI_VHSACR_HSA_MASK, prv_lbc(t->hsync_len, lbc_hz, px_hz)));
  prv_putreg(dsi, STM32_DSI_VHBPCR_OFFSET,
             PBL_FIELD_PREP(DSI_VHBPCR_HBP_MASK, prv_lbc(t->hback_porch, lbc_hz, px_hz)));
  prv_putreg(dsi, STM32_DSI_VLCR_OFFSET,
             PBL_FIELD_PREP(DSI_VLCR_HLINE_MASK, prv_lbc((uint32_t)t->hsync_len + t->hback_porch +
                                                             t->hactive + t->hfront_porch,
                                                         lbc_hz, px_hz)));
  prv_putreg(dsi, STM32_DSI_VVSACR_OFFSET, PBL_FIELD_PREP(DSI_VVSACR_VSA_MASK, t->vsync_len));
  prv_putreg(dsi, STM32_DSI_VVBPCR_OFFSET, PBL_FIELD_PREP(DSI_VVBPCR_VBP_MASK, t->vback_porch));
  prv_putreg(dsi, STM32_DSI_VVFPCR_OFFSET, PBL_FIELD_PREP(DSI_VVFPCR_VFP_MASK, t->vfront_porch));
  prv_putreg(dsi, STM32_DSI_VVACR_OFFSET, PBL_FIELD_PREP(DSI_VVACR_VA_MASK, t->vactive));
}

// Adapted command mode: the wrapper sends a frame of the display controller on each refresh, the
// panel reporting its tearing effect over the link
static void prv_command_mode(const struct pbl_mipi_dsi_stm32 *dsi,
                             const struct pbl_mipi_dsi_device *dev) {
  uint32_t color = prv_color(dev->format);

  pbl_modifyl(0U, DSI_MCR_CMDM, dsi->base + STM32_DSI_MCR_OFFSET);
  prv_putreg(dsi, STM32_DSI_WCFGR_OFFSET,
             DSI_WCFGR_DSIM | PBL_FIELD_PREP(DSI_WCFGR_COLMUX_MASK, color));
  prv_putreg(dsi, STM32_DSI_LCOLCR_OFFSET, PBL_FIELD_PREP(DSI_LCOLCR_COLC_MASK, color));
  prv_putreg(dsi, STM32_DSI_LPCR_OFFSET, 0U);
  prv_putreg(dsi, STM32_DSI_LVCIDR_OFFSET, dev->channel);
  prv_putreg(dsi, STM32_DSI_LCCR_OFFSET,
             PBL_FIELD_PREP(DSI_LCCR_CMDSIZE_MASK, dev->timing->hactive));
  pbl_modifyl(0U, DSI_CMCR_TEARE, dsi->base + STM32_DSI_CMCR_OFFSET);
  // Between frames every lane rests in the stop state, which holds while Stop stops the clocks
  pbl_modifyl(0U, DSI_CLCR_ACR, dsi->base + STM32_DSI_CLCR_OFFSET);
  pbl_modifyl(0U, DSI_WPCR1_FLPRXLPM, dsi->base + STM32_DSI_WPCR1_OFFSET);
  prv_putreg(dsi, STM32_DSI_WIFCR_OFFSET, DSI_WIRQ_TE | DSI_WIRQ_ER);
  prv_putreg(dsi, STM32_DSI_WIER_OFFSET, DSI_WIRQ_TE | DSI_WIRQ_ER);
}

static int prv_attach(const struct pbl_mipi_dsi_host *host, const struct pbl_mipi_dsi_device *dev) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_dsi(host);
  const bool video = (dev->mode_flags & PBL_MIPI_DSI_MODE_VIDEO) != 0U;
  uint32_t ref_hz;
  uint32_t px_hz = 0U;
  uint32_t lbc_hz;
  int ret;

  if (dev->lanes > 2U || dev->timing == nullptr) {
    return -ENOTSUP;
  }

  ret = prv_rate_hz(&dsi->ref, &ref_hz);
  if (ret != 0) {
    return ret;
  }

  if (video) {
    ret = prv_rate_hz(&dsi->px_clk, &px_hz);
    if (ret != 0) {
      return ret;
    }

    // The link must carry the pixels of a line within the line
    if ((uint64_t)px_hz * prv_bpp(dev->format) > (uint64_t)LANE_BIT_RATE_HZ * dev->lanes) {
      return -ENOTSUP;
    }
  }

  lbc_hz = LANE_BIT_RATE_HZ / 8U;

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
  prv_putreg(dsi, STM32_DSI_PCONFR_OFFSET,
             PBL_FIELD_PREP(DSI_PCONFR_NL_MASK, dev->lanes - 1U) |
                 PBL_FIELD_PREP(DSI_PCONFR_SW_TIME_MASK, STOP_WAIT_TIME));
  prv_putreg(
      dsi, STM32_DSI_CCR_OFFSET,
      PBL_FIELD_PREP(DSI_CCR_TXECKDIV_MASK, (lbc_hz + ESC_CLK_MAX_HZ - 1U) / ESC_CLK_MAX_HZ));
  prv_putreg(dsi, STM32_DSI_CLTCR_OFFSET,
             PBL_FIELD_PREP(DSI_CLTCR_LP2HS_TIME_MASK, LANE_SWITCH_TIME) |
                 PBL_FIELD_PREP(DSI_CLTCR_HS2LP_TIME_MASK, LANE_SWITCH_TIME));
  prv_putreg(dsi, STM32_DSI_DLTCR_OFFSET,
             PBL_FIELD_PREP(DSI_DLTCR_LP2HS_TIME_MASK, LANE_SWITCH_TIME) |
                 PBL_FIELD_PREP(DSI_DLTCR_HS2LP_TIME_MASK, LANE_SWITCH_TIME));
  prv_putreg(dsi, STM32_DSI_PCR_OFFSET, DSI_PCR_BTAE);

  if (video) {
    prv_video_mode(dsi, dev, lbc_hz, px_hz);
  } else {
    prv_command_mode(dsi, dev);
  }

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

  // The PLL locks again after Stop
  ret = prv_wait(dsi, STM32_DSI_WISR_OFFSET, DSI_WISR_PLLLS, DSI_WISR_PLLLS, PLL_TIMEOUT_US);
  if (ret != 0) {
    return ret;
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

// The commands before a refresh, as the window of the frame, must be out before its pixels
static int prv_refresh(const struct pbl_mipi_dsi_host *host) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_dsi(host);
  int ret;

  ret = prv_wait(dsi, STM32_DSI_WISR_OFFSET, DSI_WISR_PLLLS, DSI_WISR_PLLLS, PLL_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  ret = prv_wait(dsi, STM32_DSI_GPSR_OFFSET, DSI_GPSR_CMDFE, DSI_GPSR_CMDFE, CMD_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  pbl_modifyl(0U, DSI_WCR_LTDCEN, dsi->base + STM32_DSI_WCR_OFFSET);

  return 0;
}

// The wrapper lets the LTDC be set up while it is disabled
static void prv_hold(const struct pbl_mipi_dsi_host *host, bool hold) {
  const struct pbl_mipi_dsi_stm32 *dsi = prv_dsi(host);

  pbl_modifyl(hold ? DSI_WCR_DSIEN : 0U, hold ? 0U : DSI_WCR_DSIEN,
              dsi->base + STM32_DSI_WCR_OFFSET);
}

static void prv_set_event_cb(const struct pbl_mipi_dsi_host *host, pbl_mipi_dsi_event_cb_t cb,
                             void *arg) {
  struct pbl_mipi_dsi_stm32_state *st = prv_dsi(host)->state;

  pbl_irq_lock();
  st->cb = cb;
  st->arg = arg;
  pbl_irq_unlock();
}

void pbl_mipi_dsi_stm32_irq_handler(const struct pbl_mipi_dsi_stm32 *dsi) {
  struct pbl_mipi_dsi_stm32_state *st = dsi->state;
  uint32_t wisr = prv_getreg(dsi, STM32_DSI_WISR_OFFSET) & prv_getreg(dsi, STM32_DSI_WIER_OFFSET);

  prv_putreg(dsi, STM32_DSI_WIFCR_OFFSET, wisr);

  if (st->cb == nullptr) {
    return;
  }

  if ((wisr & DSI_WIRQ_TE) != 0U) {
    st->cb(PBL_MIPI_DSI_EVENT_TEAR, st->arg);
  }

  if ((wisr & DSI_WIRQ_ER) != 0U) {
    st->cb(PBL_MIPI_DSI_EVENT_REFRESH_DONE, st->arg);
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
  .refresh = prv_refresh,
  .hold = prv_hold,
  .set_event_cb = prv_set_event_cb,
};

#include PBL_DT_INSTANCES(st_stm32_dsi)
