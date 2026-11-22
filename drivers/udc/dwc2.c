/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "dwc2_regs.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/regulator.h>
#include <pbl/drivers/udc.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

// Endpoints in each direction the driver keeps state for, endpoint 0 included
#define MAX_EPS 9U

// Endpoint 0 takes packets of 64 bytes, and transfers of 3 of them at most
#define EP0_MPS     64U
#define EP0_MAX_LEN 127U

#define RESET_TIMEOUT_US 200000U
#define MODE_TIMEOUT_US  50000U

// Turnaround time, in PHY clocks: for the 60 MHz of a ULPI PHY, and for full speed on a fast AHB
#define TRDT_HS 9U
#define TRDT_FS 6U

// Variants
#define DWC2_STM32 PBL_BIT(0)

struct prv_ep {
  uint8_t *buf;
  size_t len;
  // Pushed to the FIFO for IN, received for OUT
  size_t done;
  uint16_t mps;
  bool busy;
};

struct pbl_udc_dwc2_state {
  struct prv_ep in[MAX_EPS];
  struct prv_ep out[MAX_EPS];
  uint32_t setup[2];
};

struct pbl_udc_dwc2 {
  struct pbl_udc udc;
  uintptr_t base;
  pbl_irq_t irq;
  uint32_t quirks;
  struct pbl_clk clk;
  // The clock of an external ULPI PHY, which the controller then drives at high speed
  struct pbl_clk phy_clk;
  bool ulpi;
  const struct pbl_regulator *supply;
  const struct pbl_pinctrl *pinctrl;
  // FIFO sizes, in 32-bit words: the shared receive FIFO, then each IN endpoint's
  uint16_t rx_fifo;
  uint16_t tx_fifo[MAX_EPS];
  struct pbl_udc_dwc2_state *state;
};

static const struct pbl_udc_dwc2 *prv_dwc2(const struct pbl_udc *udc) {
  return PBL_CONTAINER_OF(udc, const struct pbl_udc_dwc2, udc);
}

static inline uint32_t prv_getreg(const struct pbl_udc_dwc2 *d, uint32_t offset) {
  return pbl_readl(d->base + offset);
}

static inline void prv_putreg(const struct pbl_udc_dwc2 *d, uint32_t offset, uint32_t value) {
  pbl_writel(value, d->base + offset);
}

static inline void prv_modify(const struct pbl_udc_dwc2 *d, uint32_t offset, uint32_t clear,
                              uint32_t set) {
  pbl_modifyl(clear, set, d->base + offset);
}

static int prv_wait(const struct pbl_udc_dwc2 *d, uint32_t offset, uint32_t bits, bool set,
                    uint32_t timeout_us) {
  return PBL_POLL_BUSY_US(((prv_getreg(d, offset) & bits) == bits) == set, 1U, timeout_us);
}

static int prv_core_reset(const struct pbl_udc_dwc2 *d) {
  int ret;

  ret = prv_wait(d, DWC2_GRSTCTL, GRSTCTL_AHBIDL, true, RESET_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  prv_modify(d, DWC2_GRSTCTL, 0U, GRSTCTL_CSRST);

  return prv_wait(d, DWC2_GRSTCTL, GRSTCTL_CSRST, false, RESET_TIMEOUT_US);
}

static void prv_flush_tx(const struct pbl_udc_dwc2 *d, uint32_t fifo) {
  prv_putreg(d, DWC2_GRSTCTL, GRSTCTL_TXFFLSH | PBL_FIELD_PREP(GRSTCTL_TXFNUM_MASK, fifo));
  (void)prv_wait(d, DWC2_GRSTCTL, GRSTCTL_TXFFLSH, false, RESET_TIMEOUT_US);
}

static void prv_flush_rx(const struct pbl_udc_dwc2 *d) {
  prv_putreg(d, DWC2_GRSTCTL, GRSTCTL_RXFFLSH);
  (void)prv_wait(d, DWC2_GRSTCTL, GRSTCTL_RXFFLSH, false, RESET_TIMEOUT_US);
}

// Endpoint 0 always takes up to three back-to-back setup packets
static void prv_ep0_arm_setup(const struct pbl_udc_dwc2 *d) {
  prv_putreg(d, DWC2_DOEPTSIZ(0U),
             PBL_FIELD_PREP(DOEPTSIZ_STUPCNT_MASK, 3U) | PBL_FIELD_PREP(DEPTSIZ_PKTCNT_MASK, 1U) |
                 PBL_FIELD_PREP(DEPTSIZ_XFRSIZ_MASK, 3U * 8U));
}

static void prv_read_fifo(const struct pbl_udc_dwc2 *d, uint8_t *buf, size_t len, size_t keep) {
  for (size_t i = 0U; i < len; i += sizeof(uint32_t)) {
    uint32_t word = prv_getreg(d, DWC2_FIFO(0U));
    size_t n = PBL_MIN(len - i, sizeof(uint32_t));

    if (i < keep) {
      memcpy(&buf[i], &word, PBL_MIN(n, keep - i));
    }
  }
}

// What fits of the transfer goes to the FIFO, a packet at a time
static void prv_tx_fill(const struct pbl_udc_dwc2 *d, uint8_t n) {
  struct prv_ep *ep = &d->state->in[n];

  while (ep->done < ep->len) {
    size_t chunk = PBL_MIN(ep->len - ep->done, (size_t)ep->mps);
    size_t words = (chunk + 3U) / 4U;

    if (PBL_FIELD_GET(DTXFSTS_INEPTFSAV_MASK, prv_getreg(d, DWC2_DTXFSTS(n))) < words) {
      return;
    }

    for (size_t i = 0U; i < chunk; i += sizeof(uint32_t)) {
      uint32_t word = 0U;

      memcpy(&word, &ep->buf[ep->done + i], PBL_MIN(chunk - i, sizeof(uint32_t)));
      prv_putreg(d, DWC2_FIFO(n), word);
    }
    ep->done += chunk;
  }

  prv_modify(d, DWC2_DIEPEMPMSK, PBL_BIT(n), 0U);
}

static void prv_rx(const struct pbl_udc_dwc2 *d) {
  while ((prv_getreg(d, DWC2_GINTSTS) & GINT_RXFLVL) != 0U) {
    uint32_t sts = prv_getreg(d, DWC2_GRXSTSP);
    uint8_t n = (uint8_t)PBL_FIELD_GET(GRXSTSP_EPNUM_MASK, sts);
    size_t len = PBL_FIELD_GET(GRXSTSP_BCNT_MASK, sts);
    struct prv_ep *ep = &d->state->out[n];

    switch (PBL_FIELD_GET(GRXSTSP_PKTSTS_MASK, sts)) {
      case GRXSTSP_PKTSTS_SETUP:
        prv_read_fifo(d, (uint8_t *)d->state->setup, len, sizeof(d->state->setup));
        break;
      case GRXSTSP_PKTSTS_OUT: {
        size_t room = ep->busy ? ep->len - ep->done : 0U;

        prv_read_fifo(d, ep->busy ? &ep->buf[ep->done] : nullptr, len, room);
        ep->done += PBL_MIN(len, room);
        break;
      }
      default:
        break;
    }
  }
}

static void prv_done(const struct pbl_udc_dwc2 *d, uint8_t addr, struct prv_ep *ep, int ret) {
  ep->busy = false;
  pbl_udc_xfer_done(&d->udc, addr, ret);
}

static void prv_oep(const struct pbl_udc_dwc2 *d) {
  uint32_t daint = prv_getreg(d, DWC2_DAINT) & prv_getreg(d, DWC2_DAINTMSK);

  for (uint8_t n = 0U; n < d->udc.num_eps; n++) {
    struct prv_ep *ep = &d->state->out[n];
    uint32_t sts;

    if ((daint & DAINT_OUT(n)) == 0U) {
      continue;
    }

    sts = prv_getreg(d, DWC2_DOEPINT(n));
    prv_putreg(d, DWC2_DOEPINT(n), sts);

    if ((sts & DEPINT_XFRC) != 0U && ep->busy) {
      prv_done(d, n, ep, (int)ep->done);
    }

    if (n == 0U && (sts & DOEPINT_STUP) != 0U) {
      prv_ep0_arm_setup(d);
      pbl_udc_event(&d->udc, PBL_UDC_EVENT_SETUP, (const uint8_t *)d->state->setup);
    }
  }
}

static void prv_iep(const struct pbl_udc_dwc2 *d) {
  uint32_t daint = prv_getreg(d, DWC2_DAINT) & prv_getreg(d, DWC2_DAINTMSK);
  uint32_t empty = prv_getreg(d, DWC2_DIEPEMPMSK);

  for (uint8_t n = 0U; n < d->udc.num_eps; n++) {
    struct prv_ep *ep = &d->state->in[n];
    uint32_t sts;

    if ((daint & DAINT_IN(n)) == 0U) {
      continue;
    }

    sts = prv_getreg(d, DWC2_DIEPINT(n));
    if ((sts & DIEPINT_TXFE) != 0U && (empty & PBL_BIT(n)) != 0U) {
      prv_tx_fill(d, n);
    }

    if ((sts & DEPINT_XFRC) != 0U) {
      prv_putreg(d, DWC2_DIEPINT(n), DEPINT_XFRC);
      if (ep->busy) {
        prv_done(d, n | PBL_UDC_EP_IN, ep, (int)ep->len);
      }
    }
  }
}

// The host reset the bus: every endpoint but endpoint 0 is gone, and endpoint 0 waits for setup
static void prv_bus_reset(const struct pbl_udc_dwc2 *d) {
  prv_flush_tx(d, GRSTCTL_TXFNUM_ALL);

  for (uint8_t n = 0U; n < d->udc.num_eps; n++) {
    prv_putreg(d, DWC2_DIEPINT(n), UINT32_MAX);
    prv_putreg(d, DWC2_DOEPINT(n), UINT32_MAX);
    prv_modify(d, DWC2_DOEPCTL(n), 0U, DEPCTL_SNAK);
    if (n != 0U) {
      prv_modify(d, DWC2_DIEPCTL(n), DEPCTL_USBAEP, 0U);
      prv_modify(d, DWC2_DOEPCTL(n), DEPCTL_USBAEP, 0U);
    }
    if (d->state->in[n].busy) {
      prv_done(d, n | PBL_UDC_EP_IN, &d->state->in[n], -ECONNRESET);
    }
    if (d->state->out[n].busy) {
      prv_done(d, n, &d->state->out[n], -ECONNRESET);
    }
  }

  prv_putreg(d, DWC2_DAINTMSK, DAINT_IN(0U) | DAINT_OUT(0U));
  prv_putreg(d, DWC2_DOEPMSK, DEPINT_XFRC | DOEPINT_STUP);
  prv_putreg(d, DWC2_DIEPMSK, DEPINT_XFRC);
  prv_putreg(d, DWC2_DIEPEMPMSK, 0U);
  prv_modify(d, DWC2_DCFG, DCFG_DAD_MASK, 0U);

  d->state->in[0].mps = EP0_MPS;
  d->state->out[0].mps = EP0_MPS;
  prv_ep0_arm_setup(d);

  pbl_udc_event(&d->udc, PBL_UDC_EVENT_RESET, nullptr);
}

static enum pbl_udc_speed prv_speed(const struct pbl_udc *udc) {
  const struct pbl_udc_dwc2 *d = prv_dwc2(udc);

  return PBL_FIELD_GET(DSTS_ENUMSPD_MASK, prv_getreg(d, DWC2_DSTS)) == DSTS_ENUMSPD_HS
             ? PBL_UDC_SPEED_HIGH
             : PBL_UDC_SPEED_FULL;
}

static void prv_enum_done(const struct pbl_udc_dwc2 *d) {
  bool high = prv_speed(&d->udc) == PBL_UDC_SPEED_HIGH;

  prv_modify(d, DWC2_DIEPCTL(0U), DEPCTL_MPSIZ_MASK, DEPCTL_EP0_MPS_64);
  prv_modify(d, DWC2_GUSBCFG, GUSBCFG_TRDT_MASK,
             PBL_FIELD_PREP(GUSBCFG_TRDT_MASK, high ? TRDT_HS : TRDT_FS));
  prv_modify(d, DWC2_DCTL, 0U, DCTL_CGINAK);

  pbl_udc_event(&d->udc, PBL_UDC_EVENT_ENUMERATED, nullptr);
}

static void prv_isr(const struct pbl_udc_dwc2 *d) {
  uint32_t sts = prv_getreg(d, DWC2_GINTSTS) & prv_getreg(d, DWC2_GINTMSK);

  if ((sts & GINT_USBRST) != 0U) {
    prv_putreg(d, DWC2_GINTSTS, GINT_USBRST);
    prv_bus_reset(d);
  }

  if ((sts & GINT_ENUMDNE) != 0U) {
    prv_putreg(d, DWC2_GINTSTS, GINT_ENUMDNE);
    prv_enum_done(d);
  }

  if ((sts & GINT_USBSUSP) != 0U) {
    prv_putreg(d, DWC2_GINTSTS, GINT_USBSUSP);
    pbl_udc_event(&d->udc, PBL_UDC_EVENT_SUSPEND, nullptr);
  }

  if ((sts & GINT_WKUINT) != 0U) {
    prv_putreg(d, DWC2_GINTSTS, GINT_WKUINT);
    pbl_udc_event(&d->udc, PBL_UDC_EVENT_RESUME, nullptr);
  }

  if ((sts & GINT_RXFLVL) != 0U) {
    prv_rx(d);
  }

  if ((sts & GINT_OEPINT) != 0U) {
    prv_oep(d);
  }

  if ((sts & GINT_IEPINT) != 0U) {
    prv_iep(d);
  }
}

static int prv_set_address(const struct pbl_udc *udc, uint8_t addr) {
  prv_modify(prv_dwc2(udc), DWC2_DCFG, DCFG_DAD_MASK, PBL_FIELD_PREP(DCFG_DAD_MASK, addr));

  return 0;
}

static int prv_ep_enable(const struct pbl_udc *udc, uint8_t addr, enum pbl_udc_ep_type type,
                         uint16_t mps) {
  const struct pbl_udc_dwc2 *d = prv_dwc2(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  uint32_t ctl;

  if (n >= MAX_EPS) {
    return -EINVAL;
  }

  if (in) {
    d->state->in[n].mps = mps;
  } else {
    d->state->out[n].mps = mps;
  }

  // Endpoint 0 is the driver's
  if (n == 0U) {
    return 0;
  }

  ctl = PBL_FIELD_PREP(DEPCTL_MPSIZ_MASK, mps) | PBL_FIELD_PREP(DEPCTL_EPTYP_MASK, type) |
        DEPCTL_SD0PID | DEPCTL_USBAEP | DEPCTL_SNAK;

  pbl_irq_lock();
  if (in) {
    prv_putreg(d, DWC2_DIEPCTL(n), ctl | PBL_FIELD_PREP(DEPCTL_TXFNUM_MASK, n));
    prv_modify(d, DWC2_DAINTMSK, 0U, DAINT_IN(n));
  } else {
    prv_putreg(d, DWC2_DOEPCTL(n), ctl);
    prv_modify(d, DWC2_DAINTMSK, 0U, DAINT_OUT(n));
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_disable(const struct pbl_udc *udc, uint8_t addr) {
  const struct pbl_udc_dwc2 *d = prv_dwc2(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  uint32_t ctl = in ? DWC2_DIEPCTL(n) : DWC2_DOEPCTL(n);
  uint32_t intr = in ? DWC2_DIEPINT(n) : DWC2_DOEPINT(n);
  struct prv_ep *ep = in ? &d->state->in[n] : &d->state->out[n];

  pbl_irq_lock();
  if ((prv_getreg(d, ctl) & DEPCTL_EPENA) != 0U) {
    prv_modify(d, ctl, 0U, DEPCTL_SNAK | DEPCTL_EPDIS);
    (void)prv_wait(d, intr, DEPINT_EPDIS, true, RESET_TIMEOUT_US);
    prv_putreg(d, intr, DEPINT_EPDIS);
  }
  if (in) {
    prv_flush_tx(d, n);
    prv_modify(d, DWC2_DIEPEMPMSK, PBL_BIT(n), 0U);
  }
  if (n != 0U) {
    prv_modify(d, ctl, DEPCTL_USBAEP, 0U);
    prv_modify(d, DWC2_DAINTMSK, in ? DAINT_IN(n) : DAINT_OUT(n), 0U);
  }
  if (ep->busy) {
    prv_done(d, addr, ep, -ECONNABORTED);
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_set_halt(const struct pbl_udc *udc, uint8_t addr, bool halt) {
  const struct pbl_udc_dwc2 *d = prv_dwc2(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  uint32_t ctl = (addr & PBL_UDC_EP_IN) != 0U ? DWC2_DIEPCTL(n) : DWC2_DOEPCTL(n);

  pbl_irq_lock();
  if (halt) {
    prv_modify(d, ctl, 0U, DEPCTL_STALL);
  } else {
    prv_modify(d, ctl, DEPCTL_STALL, n != 0U ? DEPCTL_SD0PID : 0U);
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_queue(const struct pbl_udc *udc, uint8_t addr, void *buf, size_t len) {
  const struct pbl_udc_dwc2 *d = prv_dwc2(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  struct prv_ep *ep = in ? &d->state->in[n] : &d->state->out[n];
  uint32_t pkts;
  int ret = 0;

  if (n >= MAX_EPS || ep->mps == 0U || (n == 0U && len > EP0_MAX_LEN)) {
    return -EINVAL;
  }

  pkts = len == 0U ? 1U : (uint32_t)((len + ep->mps - 1U) / ep->mps);

  pbl_irq_lock();
  if (ep->busy) {
    ret = -EBUSY;
  } else {
    ep->buf = buf;
    ep->len = len;
    ep->done = 0U;
    ep->busy = true;

    if (in) {
      prv_putreg(
          d, DWC2_DIEPTSIZ(n),
          PBL_FIELD_PREP(DEPTSIZ_PKTCNT_MASK, pkts) | PBL_FIELD_PREP(DEPTSIZ_XFRSIZ_MASK, len));
      prv_modify(d, DWC2_DIEPCTL(n), DEPCTL_STALL, DEPCTL_CNAK | DEPCTL_EPENA);
      if (len != 0U) {
        prv_modify(d, DWC2_DIEPEMPMSK, 0U, PBL_BIT(n));
      }
    } else {
      prv_putreg(d, DWC2_DOEPTSIZ(n),
                 (n == 0U ? PBL_FIELD_PREP(DOEPTSIZ_STUPCNT_MASK, 3U) : 0U) |
                     PBL_FIELD_PREP(DEPTSIZ_PKTCNT_MASK, pkts) |
                     PBL_FIELD_PREP(DEPTSIZ_XFRSIZ_MASK, pkts * ep->mps));
      prv_modify(d, DWC2_DOEPCTL(n), DEPCTL_STALL, DEPCTL_CNAK | DEPCTL_EPENA);
    }
  }
  pbl_irq_unlock();

  return ret;
}

static int prv_enable(const struct pbl_udc *udc) {
  prv_modify(prv_dwc2(udc), DWC2_DCTL, DCTL_SDIS, 0U);

  return 0;
}

static int prv_disable(const struct pbl_udc *udc) {
  prv_modify(prv_dwc2(udc), DWC2_DCTL, 0U, DCTL_SDIS);

  return 0;
}

// The PHY, then a core reset, then device mode, forced, with the session taken as valid: the
// cores of the STM32 sense no VBUS here
static int prv_core_init(const struct pbl_udc_dwc2 *d) {
  int ret;

  if (d->ulpi) {
    if ((d->quirks & DWC2_STM32) != 0U) {
      prv_modify(d, DWC2_GGPIO, STM32_GCCFG_PWRDWN, 0U);
    }
    prv_modify(
        d, DWC2_GUSBCFG,
        GUSBCFG_TSDPS | GUSBCFG_ULPIFSLS | GUSBCFG_PHYSEL | GUSBCFG_ULPIEVBUSD | GUSBCFG_ULPIEVBUSI,
        0U);
  } else {
    prv_modify(d, DWC2_GUSBCFG, 0U, GUSBCFG_PHYSEL);
  }

  ret = prv_core_reset(d);
  if (ret != 0) {
    return ret;
  }

  if (!d->ulpi && (d->quirks & DWC2_STM32) != 0U) {
    prv_modify(d, DWC2_GGPIO, 0U, STM32_GCCFG_PWRDWN);
  }

  prv_modify(d, DWC2_GUSBCFG, GUSBCFG_FHMOD, GUSBCFG_FDMOD);
  ret = prv_wait(d, DWC2_GINTSTS, GINT_CMOD, false, MODE_TIMEOUT_US);
  if (ret != 0) {
    return ret;
  }

  if ((d->quirks & DWC2_STM32) != 0U) {
    prv_modify(d, DWC2_GGPIO, STM32_GCCFG_VBDEN, 0U);
  }
  prv_modify(d, DWC2_GOTGCTL, 0U, GOTGCTL_BVALOEN | GOTGCTL_BVALOVAL);

  return 0;
}

static void prv_fifo_init(const struct pbl_udc_dwc2 *d) {
  uint32_t addr = d->rx_fifo;

  prv_putreg(d, DWC2_GRXFSIZ, d->rx_fifo);
  for (uint8_t n = 0U; n < d->udc.num_eps; n++) {
    uint32_t value = ((uint32_t)d->tx_fifo[n] << 16) | addr;

    prv_putreg(d, n == 0U ? DWC2_DIEPTXF0 : DWC2_DIEPTXF(n), value);
    addr += d->tx_fifo[n];
  }

  prv_flush_tx(d, GRSTCTL_TXFNUM_ALL);
  prv_flush_rx(d);
}

static int prv_init(const struct pbl_udc *udc) {
  const struct pbl_udc_dwc2 *d = prv_dwc2(udc);
  int ret;

  if (udc->num_eps > MAX_EPS) {
    return -EINVAL;
  }

  ret = pbl_device_init(&d->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  if (d->supply != nullptr) {
    ret = pbl_regulator_enable(d->supply);
    if (ret != 0) {
      return ret;
    }
  }

  ret = pbl_pinctrl_select_state(d->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&d->clk);
  if (ret == 0 && d->ulpi) {
    ret = pbl_clk_enable(&d->phy_clk);
  }
  if (ret != 0) {
    return ret;
  }

  ret = prv_core_init(d);
  if (ret != 0) {
    return ret;
  }

  // Disconnected until the stack connects, at the speed the PHY allows
  prv_putreg(d, DWC2_PCGCCTL, 0U);
  prv_modify(d, DWC2_DCTL, 0U, DCTL_SDIS);
  prv_modify(d, DWC2_DCFG, DCFG_DSPD_MASK,
             PBL_FIELD_PREP(DCFG_DSPD_MASK, d->ulpi ? DCFG_DSPD_HS : DCFG_DSPD_FS));

  prv_fifo_init(d);

  prv_putreg(d, DWC2_DIEPMSK, 0U);
  prv_putreg(d, DWC2_DOEPMSK, 0U);
  prv_putreg(d, DWC2_DAINTMSK, 0U);
  for (uint8_t n = 0U; n < udc->num_eps; n++) {
    prv_putreg(d, DWC2_DIEPCTL(n), (n == 0U) ? 0U : DEPCTL_SNAK);
    prv_putreg(d, DWC2_DOEPCTL(n), (n == 0U) ? 0U : DEPCTL_SNAK);
    prv_putreg(d, DWC2_DIEPINT(n), UINT32_MAX);
    prv_putreg(d, DWC2_DOEPINT(n), UINT32_MAX);
  }

  prv_putreg(d, DWC2_GINTSTS, UINT32_MAX);
  prv_putreg(d, DWC2_GINTMSK,
             GINT_USBRST | GINT_ENUMDNE | GINT_USBSUSP | GINT_WKUINT | GINT_RXFLVL | GINT_IEPINT |
                 GINT_OEPINT);
  prv_modify(d, DWC2_GAHBCFG, 0U, GAHBCFG_GINTMSK);

  pbl_irq_enable(d->irq);

  return 0;
}

static const struct pbl_udc_ops s_ops = {
  .init = prv_init,
  .enable = prv_enable,
  .disable = prv_disable,
  .speed = prv_speed,
  .set_address = prv_set_address,
  .ep_enable = prv_ep_enable,
  .ep_disable = prv_ep_disable,
  .ep_set_halt = prv_ep_set_halt,
  .ep_queue = prv_ep_queue,
};

#define PBL_UDC_DWC2_DEFINE(sym, _name, _base, _irq, _quirks, _clk, _phy_clk, _ulpi, _supply, \
                            _pinctrl, _rx_fifo, _tx0_fifo, ...)                               \
  PBL_UDC_STATE_DEFINE(sym);                                                                  \
  static struct pbl_udc_dwc2_state sym##_dwc2_state;                                          \
  static const struct pbl_pinctrl sym##_dwc2_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);        \
  PBL_DEVICE_INSTANCE(sym, struct pbl_udc_dwc2, struct pbl_udc, udc){                         \
    .udc = PBL_UDC_INIT(sym, _name, nullptr, &s_ops,                                          \
                        1U + PBL_ARRAY_SIZE(((const uint16_t[]){__VA_ARGS__})),               \
                        (_ulpi) ? PBL_UDC_SPEED_HIGH : PBL_UDC_SPEED_FULL),                   \
    .base = (_base),                                                                          \
    .irq = (_irq),                                                                            \
    .quirks = (_quirks),                                                                      \
    .clk = _clk,                                                                              \
    .phy_clk = _phy_clk,                                                                      \
    .ulpi = (_ulpi),                                                                          \
    .supply = (_supply),                                                                      \
    .pinctrl = &sym##_dwc2_pins,                                                              \
    .rx_fifo = (_rx_fifo),                                                                    \
    .tx_fifo = {_tx0_fifo, __VA_ARGS__},                                                      \
    .state = &sym##_dwc2_state,                                                               \
  };                                                                                          \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UDC_DWC2_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(st_stm32f7_hsotg)
