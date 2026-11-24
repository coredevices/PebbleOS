/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "musb_regs.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/clk/sf32lb.h>
#include <pbl/drivers/pinctrl.h>
#include <pbl/drivers/udc.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/idle.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/soc/sf32lb/idle.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

// Endpoints in each direction the driver keeps state for, endpoint 0 included
#define MAX_EPS 8U

#define EP0_MPS 64U

// Packets of the other endpoints, each with a FIFO of its own in each direction
#define EP_FIFO_LEN 64U

// SiFli: the USB PHY, in the system configuration block, which also has the analog bandgap
#define SIFLI_HPSYS_CFG_USBCR   0x3c
#define SIFLI_USBCR_USB_EN      PBL_BIT(0)
#define SIFLI_USBCR_DM_PD       PBL_BIT(5)
#define SIFLI_USBCR_DP_EN       PBL_BIT(6)
#define SIFLI_HPSYS_CFG_ANAU_CR 0x44
#define SIFLI_ANAU_CR_EN_BG     PBL_BIT(0)
#define SIFLI_BANDGAP_US        10U

// Variants
#define MUSB_SIFLI PBL_BIT(0)

// Stages of a control transfer on endpoint 0, which the controller sequences itself
enum prv_ep0 {
  EP0_IDLE,
  // A setup packet, read, whose RXPKTRDY the controller still holds
  EP0_SETUP,
  EP0_TX,
  EP0_RX,
  // DATAEND set: the status stage is the controller's
  EP0_STATUS,
};

struct prv_ep {
  uint8_t *buf;
  size_t len;
  size_t done;
  uint16_t mps;
  bool busy;
};

struct pbl_udc_musb_state {
  struct prv_ep in[MAX_EPS];
  struct prv_ep out[MAX_EPS];
  enum prv_ep0 ep0;
  // SiFli: clk_hpsys kept running in deep WFI while connected
  bool deepwfi_held;
  // The address the status stage applies
  int16_t address;
  uint8_t setup[8];
};

struct pbl_udc_musb {
  struct pbl_udc udc;
  uintptr_t base;
  pbl_irq_t irq;
  uint32_t quirks;
  struct pbl_clk clk;
  const struct pbl_clk_assigned *assigned;
  const struct pbl_pinctrl *pinctrl;
  // SiFli: the system configuration block holding the PHY controls
  uintptr_t syscfg;
  struct pbl_udc_musb_state *state;
};

static const struct pbl_udc_musb *prv_musb(const struct pbl_udc *udc) {
  return PBL_CONTAINER_OF(udc, const struct pbl_udc_musb, udc);
}

static inline uint8_t prv_rb(const struct pbl_udc_musb *m, uint32_t offset) {
  return pbl_readb(m->base + offset);
}

static inline void prv_wb(const struct pbl_udc_musb *m, uint32_t offset, uint8_t value) {
  pbl_writeb(value, m->base + offset);
}

static inline uint16_t prv_rw(const struct pbl_udc_musb *m, uint32_t offset) {
  return pbl_readw(m->base + offset);
}

static inline void prv_ww(const struct pbl_udc_musb *m, uint32_t offset, uint16_t value) {
  pbl_writew(value, m->base + offset);
}

static void prv_index(const struct pbl_udc_musb *m, uint8_t n) {
  prv_wb(m, MUSB_INDEX, n);
}

static void prv_write_fifo(const struct pbl_udc_musb *m, uint8_t n, const uint8_t *buf,
                           size_t len) {
  for (size_t i = 0U; i < len; i++) {
    pbl_writeb(buf[i], m->base + MUSB_FIFO(n));
  }
}

static void prv_read_fifo(const struct pbl_udc_musb *m, uint8_t n, uint8_t *buf, size_t len,
                          size_t keep) {
  for (size_t i = 0U; i < len; i++) {
    uint8_t byte = pbl_readb(m->base + MUSB_FIFO(n));

    if (i < keep) {
      buf[i] = byte;
    }
  }
}

static void prv_done(const struct pbl_udc_musb *m, uint8_t addr, struct prv_ep *ep, int ret) {
  ep->busy = false;
  pbl_udc_xfer_done(&m->udc, addr, ret);
}

// The next packet of an IN transfer, the last one ending the data stage on endpoint 0
static void prv_tx_packet(const struct pbl_udc_musb *m, uint8_t n) {
  struct prv_ep *ep = &m->state->in[n];
  size_t chunk = PBL_MIN(ep->len - ep->done, (size_t)ep->mps);

  prv_index(m, n);
  prv_write_fifo(m, n, &ep->buf[ep->done], chunk);
  ep->done += chunk;

  if (n == 0U) {
    // A full packet short of wLength leaves the data stage open, for the next transfer
    uint16_t wlength = (uint16_t)(m->state->setup[6] | (m->state->setup[7] << 8));
    bool last = chunk < ep->mps || ep->done >= wlength;

    prv_ww(m, MUSB_CSR0, MUSB_CSR0_TXPKTRDY | (last ? MUSB_CSR0_DATAEND : 0U));
    m->state->ep0 = last ? EP0_STATUS : EP0_TX;
  } else {
    prv_ww(m, MUSB_TXCSR, prv_rw(m, MUSB_TXCSR) | MUSB_TXCSR_TXPKTRDY);
  }
}

// A packet of an OUT transfer: the transfer ends with a short packet or once full
static void prv_rx_packet(const struct pbl_udc_musb *m, uint8_t n) {
  struct prv_ep *ep = &m->state->out[n];
  size_t len;
  size_t room = ep->len - ep->done;
  bool end;

  prv_index(m, n);
  len = (n == 0U) ? prv_rb(m, MUSB_COUNT0) : prv_rw(m, MUSB_RXCOUNT);
  prv_read_fifo(m, n, &ep->buf[ep->done], len, room);
  ep->done += PBL_MIN(len, room);
  end = len < ep->mps || ep->done >= ep->len;

  if (n == 0U) {
    prv_ww(m, MUSB_CSR0, MUSB_CSR0_SERVICEDRXPKTRDY | (end ? MUSB_CSR0_DATAEND : 0U));
    if (end) {
      m->state->ep0 = EP0_STATUS;
    }
  } else {
    prv_ww(m, MUSB_RXCSR, prv_rw(m, MUSB_RXCSR) & ~MUSB_RXCSR_RXPKTRDY);
  }

  if (end) {
    prv_done(m, n, ep, (int)ep->done);
  }
}

// A control transfer the host ended early or that was stalled: its stages will not complete
static void prv_ep0_abort(const struct pbl_udc_musb *m) {
  struct pbl_udc_musb_state *st = m->state;

  st->ep0 = EP0_IDLE;
  if (st->in[0].busy) {
    prv_done(m, PBL_UDC_EP_IN, &st->in[0], -ECONNABORTED);
  }
  if (st->out[0].busy) {
    prv_done(m, 0U, &st->out[0], -ECONNABORTED);
  }
}

static void prv_ep0_isr(const struct pbl_udc_musb *m) {
  struct pbl_udc_musb_state *st = m->state;
  uint16_t csr;

  prv_index(m, 0U);
  csr = prv_rw(m, MUSB_CSR0);

  if ((csr & MUSB_CSR0_SENTSTALL) != 0U) {
    prv_ww(m, MUSB_CSR0, csr & ~MUSB_CSR0_SENTSTALL);
    prv_ep0_abort(m);
  }

  if ((csr & MUSB_CSR0_SETUPEND) != 0U) {
    prv_ww(m, MUSB_CSR0, MUSB_CSR0_SERVICEDSETUPEND);
    prv_ep0_abort(m);
  }

  switch (st->ep0) {
    case EP0_STATUS:
      // The status stage went through: the address it carried takes effect now
      if (st->address >= 0) {
        prv_wb(m, MUSB_FADDR, (uint8_t)st->address);
        st->address = -1;
      }
      st->ep0 = EP0_IDLE;
      if (st->in[0].busy) {
        prv_done(m, PBL_UDC_EP_IN, &st->in[0], (int)st->in[0].done);
      }
      if (st->out[0].busy) {
        prv_done(m, 0U, &st->out[0], 0);
      }
      break;
    case EP0_TX:
      if (st->in[0].done < st->in[0].len) {
        prv_tx_packet(m, 0U);
      } else if (st->in[0].busy) {
        // The data stage stays open for the zero-length packet the stack queues next
        prv_done(m, PBL_UDC_EP_IN, &st->in[0], (int)st->in[0].done);
      }
      return;
    case EP0_RX:
      if ((csr & MUSB_CSR0_RXPKTRDY) != 0U) {
        prv_rx_packet(m, 0U);
      }
      return;
    default:
      break;
  }

  csr = prv_rw(m, MUSB_CSR0);
  if (st->ep0 == EP0_IDLE && (csr & MUSB_CSR0_RXPKTRDY) != 0U && prv_rb(m, MUSB_COUNT0) == 8U) {
    prv_read_fifo(m, 0U, st->setup, sizeof(st->setup), sizeof(st->setup));
    st->ep0 = EP0_SETUP;
    pbl_udc_event(&m->udc, PBL_UDC_EVENT_SETUP, st->setup);
  }
}

static void prv_bus_reset(const struct pbl_udc_musb *m) {
  struct pbl_udc_musb_state *st = m->state;

  prv_wb(m, MUSB_FADDR, 0U);
  st->address = -1;
  st->ep0 = EP0_IDLE;

  for (uint8_t n = 0U; n < m->udc.num_eps; n++) {
    if (st->in[n].busy) {
      prv_done(m, n | PBL_UDC_EP_IN, &st->in[n], -ECONNRESET);
    }
    if (st->out[n].busy) {
      prv_done(m, n, &st->out[n], -ECONNRESET);
    }
  }

  for (uint8_t n = 1U; n < MAX_EPS; n++) {
    st->in[n].mps = 0U;
    st->out[n].mps = 0U;
  }
  prv_ww(m, MUSB_INTRTXE, PBL_BIT(0));
  prv_ww(m, MUSB_INTRRXE, 0U);

  pbl_udc_event(&m->udc, PBL_UDC_EVENT_RESET, nullptr);
  pbl_udc_event(&m->udc, PBL_UDC_EVENT_ENUMERATED, nullptr);
}

static void prv_isr(const struct pbl_udc_musb *m) {
  uint8_t usb = prv_rb(m, MUSB_INTRUSB);
  uint16_t tx = prv_rw(m, MUSB_INTRTX);
  uint16_t rx = prv_rw(m, MUSB_INTRRX);

  if ((usb & MUSB_INTRUSB_RESET) != 0U) {
    prv_bus_reset(m);
  }
  if ((usb & MUSB_INTRUSB_SUSPEND) != 0U) {
    pbl_udc_event(&m->udc, PBL_UDC_EVENT_SUSPEND, nullptr);
  }
  if ((usb & MUSB_INTRUSB_RESUME) != 0U) {
    pbl_udc_event(&m->udc, PBL_UDC_EVENT_RESUME, nullptr);
  }

  if ((tx & PBL_BIT(0)) != 0U) {
    prv_ep0_isr(m);
  }

  for (uint8_t n = 1U; n < m->udc.num_eps; n++) {
    struct prv_ep *in = &m->state->in[n];
    struct prv_ep *out = &m->state->out[n];

    if ((tx & PBL_BIT(n)) != 0U && in->busy) {
      if (in->done < in->len) {
        prv_tx_packet(m, n);
      } else {
        prv_done(m, n | PBL_UDC_EP_IN, in, (int)in->len);
      }
    }

    if ((rx & PBL_BIT(n)) != 0U && out->busy) {
      prv_rx_packet(m, n);
    }
  }
}

static enum pbl_udc_speed prv_speed(const struct pbl_udc *udc) {
  return (prv_rb(prv_musb(udc), MUSB_POWER) & MUSB_POWER_HSMODE) != 0U ? PBL_UDC_SPEED_HIGH
                                                                       : PBL_UDC_SPEED_FULL;
}

// The address takes effect once the status stage of its request went through
static int prv_set_address(const struct pbl_udc *udc, uint8_t addr) {
  prv_musb(udc)->state->address = addr;

  return 0;
}

static int prv_ep_enable(const struct pbl_udc *udc, uint8_t addr, enum pbl_udc_ep_type type,
                         uint16_t mps) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  bool iso = type == PBL_UDC_EP_ISOCHRONOUS;

  if (n >= MAX_EPS || mps > EP_FIFO_LEN) {
    return -EINVAL;
  }

  // Each endpoint number serves one direction at a time, the one TXCSR.MODE selects
  if (n != 0U && (in ? m->state->out[n].mps : m->state->in[n].mps) != 0U) {
    return -EBUSY;
  }

  if (in) {
    m->state->in[n].mps = mps;
  } else {
    m->state->out[n].mps = mps;
  }

  if (n == 0U) {
    return 0;
  }

  pbl_irq_lock();
  prv_index(m, n);
  if (in) {
    prv_ww(m, MUSB_TXMAXP, mps);
    prv_ww(m, MUSB_TXCSR,
           MUSB_TXCSR_MODE | MUSB_TXCSR_CLRDATATOG | MUSB_TXCSR_FLUSHFIFO |
               (iso ? MUSB_TXCSR_ISO : 0U));
    prv_ww(m, MUSB_INTRTXE, prv_rw(m, MUSB_INTRTXE) | PBL_BIT(n));
  } else {
    prv_ww(m, MUSB_TXCSR, 0U);
    prv_ww(m, MUSB_RXMAXP, mps);
    prv_ww(m, MUSB_RXCSR,
           MUSB_RXCSR_CLRDATATOG | MUSB_RXCSR_FLUSHFIFO | (iso ? MUSB_RXCSR_ISO : 0U));
    prv_ww(m, MUSB_INTRRXE, prv_rw(m, MUSB_INTRRXE) | PBL_BIT(n));
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_disable(const struct pbl_udc *udc, uint8_t addr) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  struct prv_ep *ep = in ? &m->state->in[n] : &m->state->out[n];

  pbl_irq_lock();
  if (n != 0U) {
    prv_index(m, n);
    if (in) {
      prv_ww(m, MUSB_INTRTXE, prv_rw(m, MUSB_INTRTXE) & ~PBL_BIT(n));
      prv_ww(m, MUSB_TXCSR, MUSB_TXCSR_MODE | MUSB_TXCSR_FLUSHFIFO);
    } else {
      prv_ww(m, MUSB_INTRRXE, prv_rw(m, MUSB_INTRRXE) & ~PBL_BIT(n));
      prv_ww(m, MUSB_RXCSR, MUSB_RXCSR_FLUSHFIFO);
    }
  }
  if (ep->busy) {
    prv_done(m, addr, ep, -ECONNABORTED);
  }
  if (n != 0U) {
    ep->mps = 0U;
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_set_halt(const struct pbl_udc *udc, uint8_t addr, bool halt) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;

  pbl_irq_lock();
  prv_index(m, n);
  if (n == 0U) {
    // A request the stack refuses: its setup packet is served with a stall
    if (halt && m->state->ep0 == EP0_SETUP) {
      prv_ww(m, MUSB_CSR0, MUSB_CSR0_SERVICEDRXPKTRDY | MUSB_CSR0_SENDSTALL);
      m->state->ep0 = EP0_IDLE;
    }
  } else if ((addr & PBL_UDC_EP_IN) != 0U) {
    uint16_t csr = prv_rw(m, MUSB_TXCSR) & ~(MUSB_TXCSR_SENDSTALL | MUSB_TXCSR_SENTSTALL);

    prv_ww(m, MUSB_TXCSR, halt ? csr | MUSB_TXCSR_SENDSTALL : csr | MUSB_TXCSR_CLRDATATOG);
  } else {
    uint16_t csr = prv_rw(m, MUSB_RXCSR) & ~(MUSB_RXCSR_SENDSTALL | MUSB_RXCSR_SENTSTALL);

    prv_ww(m, MUSB_RXCSR, halt ? csr | MUSB_RXCSR_SENDSTALL : csr | MUSB_RXCSR_CLRDATATOG);
  }
  pbl_irq_unlock();

  return 0;
}

// Endpoint 0 follows the stage the stack is at: a data stage starts once the setup packet is
// served, and a status stage the controller already runs completes with it
static int prv_ep0_queue(const struct pbl_udc_musb *m, bool in, struct prv_ep *ep) {
  struct pbl_udc_musb_state *st = m->state;

  prv_index(m, 0U);
  switch (st->ep0) {
    case EP0_SETUP:
      // An IN data stage may be empty, a zero-length packet, when the request has data
      if (ep->len == 0U && !(in && (st->setup[6] != 0U || st->setup[7] != 0U))) {
        prv_ww(m, MUSB_CSR0, MUSB_CSR0_SERVICEDRXPKTRDY | MUSB_CSR0_DATAEND);
        st->ep0 = EP0_STATUS;
      } else if (in) {
        prv_ww(m, MUSB_CSR0, MUSB_CSR0_SERVICEDRXPKTRDY);
        prv_tx_packet(m, 0U);
      } else {
        prv_ww(m, MUSB_CSR0, MUSB_CSR0_SERVICEDRXPKTRDY);
        st->ep0 = EP0_RX;
      }
      return 0;
    case EP0_TX:
      // The zero-length packet ending a data stage of full packets
      if (!in || ep->len != 0U) {
        return -EBUSY;
      }
      prv_tx_packet(m, 0U);
      return 0;
    case EP0_STATUS:
      return ep->len == 0U ? 0 : -EBUSY;
    case EP0_IDLE:
      // The status stage went through before the stack queued it
      if (ep->len != 0U) {
        return -EBUSY;
      }
      prv_done(m, in ? PBL_UDC_EP_IN : 0U, ep, 0);
      return 0;
    default:
      return -EBUSY;
  }
}

static int prv_ep_queue(const struct pbl_udc *udc, uint8_t addr, void *buf, size_t len) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  struct prv_ep *ep = in ? &m->state->in[n] : &m->state->out[n];
  int ret = 0;

  if (n >= MAX_EPS || ep->mps == 0U) {
    return -EINVAL;
  }

  pbl_irq_lock();
  if (ep->busy) {
    ret = -EBUSY;
  } else {
    ep->buf = buf;
    ep->len = len;
    ep->done = 0U;
    ep->busy = true;

    if (n == 0U) {
      ret = prv_ep0_queue(m, in, ep);
      if (ret != 0) {
        ep->busy = false;
      }
    } else if (in) {
      prv_tx_packet(m, n);
    } else {
      // A packet the host sent while none was queued waits in the FIFO, NAKing the next ones
      prv_index(m, n);
      if ((prv_rw(m, MUSB_RXCSR) & MUSB_RXCSR_RXPKTRDY) != 0U) {
        prv_rx_packet(m, n);
      }
    }
  }
  pbl_irq_unlock();

  return ret;
}

static int prv_enable(const struct pbl_udc *udc) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t power = prv_rb(m, MUSB_POWER);

  if ((power & MUSB_POWER_SOFTCONN) != 0U) {
    return 0;
  }

  // SiFli: in deep WFI the controller needs clk_hpsys kept running, and deep sleep stops it
  if ((m->quirks & MUSB_SIFLI) != 0U) {
    m->state->deepwfi_held = pbl_clk_sf32lb_deepwfi_hold() == 0;
    pbl_idle_state_block(m->state->deepwfi_held ? SOC_SF32LB_IDLE_DEEPSLEEP
                                                : SOC_SF32LB_IDLE_DEEPWFI);
  }
  prv_wb(m, MUSB_POWER, power | MUSB_POWER_SOFTCONN);

  return 0;
}

static int prv_disable(const struct pbl_udc *udc) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t power = prv_rb(m, MUSB_POWER);

  if ((power & MUSB_POWER_SOFTCONN) == 0U) {
    return 0;
  }

  prv_wb(m, MUSB_POWER, power & ~MUSB_POWER_SOFTCONN);
  if ((m->quirks & MUSB_SIFLI) != 0U) {
    pbl_idle_state_release(m->state->deepwfi_held ? SOC_SF32LB_IDLE_DEEPSLEEP
                                                  : SOC_SF32LB_IDLE_DEEPWFI);
    if (m->state->deepwfi_held) {
      pbl_clk_sf32lb_deepwfi_release();
    }
  }

  return 0;
}

// Endpoint 0 has the first 64 bytes of the FIFO RAM, then each endpoint a FIFO in each direction
static int prv_fifo_init(const struct pbl_udc_musb *m) {
  uint32_t ram = 4U << PBL_FIELD_GET(MUSB_RAMINFO_RAMBITS_MASK, prv_rb(m, MUSB_RAMINFO));
  uint32_t addr = EP0_MPS;
  uint8_t sz = 0U;

  while ((8U << sz) < EP_FIFO_LEN) {
    sz++;
  }

  if (addr + 2U * EP_FIFO_LEN * (m->udc.num_eps - 1U) > ram) {
    return -ENOMEM;
  }

  for (uint8_t n = 1U; n < m->udc.num_eps; n++) {
    prv_index(m, n);
    prv_wb(m, MUSB_TXFIFOSZ, sz);
    prv_ww(m, MUSB_TXFIFOADD, (uint16_t)(addr / 8U));
    addr += EP_FIFO_LEN;
    prv_wb(m, MUSB_RXFIFOSZ, sz);
    prv_ww(m, MUSB_RXFIFOADD, (uint16_t)(addr / 8U));
    addr += EP_FIFO_LEN;
  }

  // One packet per FIFO: no double buffering
  prv_ww(m, MUSB_RXDPKTBUFDIS, (uint16_t)~PBL_BIT(0));
  prv_ww(m, MUSB_TXDPKTBUFDIS, (uint16_t)~PBL_BIT(0));

  return 0;
}

// SiFli: the bandgap, the controller clock, then the PHY with its pull-up on D+, its session
// valid inputs driven by the controller. The controller runs from its 60 MHz kernel clock, or
// else from the 48 MHz crystal.
static void prv_sifli_phy_on(const struct pbl_udc_musb *m, uint64_t rate_mhz) {
  pbl_modifyl(0U, SIFLI_ANAU_CR_EN_BG, m->syscfg + SIFLI_HPSYS_CFG_ANAU_CR);
  pbl_busy_wait_us(SIFLI_BANDGAP_US);
  pbl_writeb(rate_mhz == SIFLI_USBC_60M_RATE ? SIFLI_USBC_MODE_60M : SIFLI_USBC_MODE_48M_XTAL,
             m->base + SIFLI_USBC_MODE_48M);
  pbl_modifyl(0U, SIFLI_USBCR_DM_PD | SIFLI_USBCR_DP_EN | SIFLI_USBCR_USB_EN,
              m->syscfg + SIFLI_HPSYS_CFG_USBCR);
  pbl_writeb(pbl_readb(m->base + SIFLI_USBC_USBCFG) | SIFLI_USBCFG_AVALID | SIFLI_USBCFG_AVALID_DR,
             m->base + SIFLI_USBC_USBCFG);
}

static int prv_init(const struct pbl_udc *udc) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint64_t rate_mhz;
  int ret;

  if (udc->num_eps > MAX_EPS) {
    return -EINVAL;
  }

  ret = pbl_device_init(&m->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_pinctrl_select_state(m->pinctrl, PBL_PINCTRL_STATE_DEFAULT);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_assign(m->assigned);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_enable(&m->clk);
  if (ret != 0) {
    return ret;
  }

  ret = pbl_clk_get_rate(&m->clk, &rate_mhz);
  if (ret != 0) {
    return ret;
  }

  if ((m->quirks & MUSB_SIFLI) != 0U) {
    prv_sifli_phy_on(m, rate_mhz);
  }

  // Disconnected, at full speed, until the stack connects
  prv_wb(m, MUSB_POWER, 0U);

  ret = prv_fifo_init(m);
  if (ret != 0) {
    return ret;
  }

  m->state->address = -1;
  m->state->in[0].mps = EP0_MPS;
  m->state->out[0].mps = EP0_MPS;

  (void)prv_rb(m, MUSB_INTRUSB);
  (void)prv_rw(m, MUSB_INTRTX);
  (void)prv_rw(m, MUSB_INTRRX);
  prv_ww(m, MUSB_INTRTXE, PBL_BIT(0));
  prv_ww(m, MUSB_INTRRXE, 0U);
  prv_wb(m, MUSB_INTRUSBE, MUSB_INTRUSB_RESET | MUSB_INTRUSB_SUSPEND | MUSB_INTRUSB_RESUME);

  pbl_irq_enable(m->irq);

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

// SiFli: endpoint 0 and seven others
#define SIFLI_NUM_EPS 8U

#define PBL_UDC_MUSB_DEFINE(sym, _name, _base, _irq, _quirks, _clk, _pinctrl, _syscfg, ...) \
  PBL_UDC_STATE_DEFINE(sym);                                                                \
  static struct pbl_udc_musb_state sym##_musb_state;                                        \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                     \
  static const struct pbl_pinctrl sym##_musb_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);      \
  PBL_DEVICE_INSTANCE(sym, struct pbl_udc_musb, struct pbl_udc, udc){                       \
    .udc = PBL_UDC_INIT(sym, _name, nullptr, &s_ops, SIFLI_NUM_EPS, PBL_UDC_SPEED_FULL),    \
    .base = (_base),                                                                        \
    .irq = (_irq),                                                                          \
    .quirks = (_quirks),                                                                    \
    .clk = _clk,                                                                            \
    .assigned = sym##_assigned,                                                             \
    .pinctrl = &sym##_musb_pins,                                                            \
    .syscfg = (_syscfg),                                                                    \
    .state = &sym##_musb_state,                                                             \
  };                                                                                        \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UDC_MUSB_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(sifli_sf32lb57_usb)
