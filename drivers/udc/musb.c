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
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

// Endpoints in each direction the driver keeps state for, endpoint 0 included
#define MAX_EPS 8U

#define EP0_MPS 64U

// Direction bit of a setup packet's bmRequestType
#define SETUP_DIR_IN 0x80U

// Largest packet of the other endpoints: full speed's, for bulk and interrupt
#define EP_MPS_MAX 64U

#define FIFO_MIN 8U

// Room a double-buffered FIFO leaves for the endpoints enabled after it: two of the largest packets
#define FIFO_RESERVE (2U * EP_MPS_MAX)

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
  // IN: the packet in the FIFO, which a stall flushes
  size_t chunk;
  uint16_t mps;
  bool busy;
  // IN: a stall flushed the packet, which goes again once the stall clears
  bool flushed;
};

// The FIFO of an endpoint number, for the direction it serves
struct prv_fifo {
  uint16_t addr;
  // Of a packet
  uint16_t size;
  // Room for two packets
  bool two;
  // Double-buffered, for a bulk OUT endpoint
  bool dpb;
};

struct pbl_udc_musb_state {
  struct prv_ep in[MAX_EPS];
  struct prv_ep out[MAX_EPS];
  struct prv_fifo fifo[MAX_EPS];
  // The FIFO RAM past the FIFOs given out
  uint16_t fifo_end;
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
  // FIFO RAM, endpoint 0's included
  uint16_t fifo_size;
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

// The calls the stack makes from a callback in the interrupt handler leave the endpoint the
// handler selected
static uint8_t prv_lock(const struct pbl_udc_musb *m) {
  pbl_irq_lock();

  return prv_rb(m, MUSB_INDEX);
}

static void prv_unlock(const struct pbl_udc_musb *m, uint8_t index) {
  prv_index(m, index);
  pbl_irq_unlock();
}

static void prv_write_fifo(const struct pbl_udc_musb *m, uint8_t n, const uint8_t *buf,
                           size_t len) {
  uintptr_t fifo = m->base + MUSB_FIFO(n);
  size_t i = 0U;

  for (; i + 4U <= len; i += 4U) {
    pbl_writel(pbl_get_le32(&buf[i]), fifo);
  }
  if (len - i >= 2U) {
    pbl_writew(pbl_get_le16(&buf[i]), fifo);
    i += 2U;
  }
  if (i < len) {
    pbl_writeb(buf[i], fifo);
  }
}

// The bytes past keep are drained from the FIFO
static void prv_read_fifo(const struct pbl_udc_musb *m, uint8_t n, uint8_t *buf, size_t len,
                          size_t keep) {
  uintptr_t fifo = m->base + MUSB_FIFO(n);
  size_t i = 0U;

  for (; i + 4U <= len; i += 4U) {
    uint32_t w = pbl_cpu_to_le32(pbl_readl(fifo));

    if (i < keep) {
      memcpy(&buf[i], &w, PBL_MIN(sizeof(w), keep - i));
    }
  }
  if (len - i >= 2U) {
    uint16_t h = pbl_cpu_to_le16(pbl_readw(fifo));

    if (i < keep) {
      memcpy(&buf[i], &h, PBL_MIN(sizeof(h), keep - i));
    }
    i += 2U;
  }
  if (i < len) {
    uint8_t b = pbl_readb(fifo);

    if (i < keep) {
      buf[i] = b;
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
  ep->chunk = chunk;

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

// Endpoint 0 keeps the first 64 bytes of the FIFO RAM
static void prv_fifo_reset(const struct pbl_udc_musb *m) {
  memset(m->state->fifo, 0, sizeof(m->state->fifo));
  m->state->fifo_end = EP0_MPS;
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
  prv_fifo_reset(m);
  prv_ww(m, MUSB_INTRTXE, PBL_BIT(0));
  prv_ww(m, MUSB_INTRRXE, 0U);

  pbl_udc_event(&m->udc, PBL_UDC_EVENT_RESET, nullptr);
  pbl_udc_event(&m->udc, PBL_UDC_EVENT_ENUMERATED, nullptr);
}

static void prv_tx_isr(const struct pbl_udc_musb *m, uint8_t n) {
  struct prv_ep *ep = &m->state->in[n];
  uint16_t csr;

  prv_index(m, n);
  csr = prv_rw(m, MUSB_TXCSR);
  if ((csr & MUSB_TXCSR_SENTSTALL) != 0U) {
    prv_ww(m, MUSB_TXCSR,
           (csr & ~(MUSB_TXCSR_SENTSTALL | MUSB_TXCSR_TXPKTRDY)) |
               ((csr & MUSB_TXCSR_TXPKTRDY) != 0U ? MUSB_TXCSR_FLUSHFIFO : 0U));
    if (ep->busy && !ep->flushed) {
      ep->done -= ep->chunk;
      ep->flushed = true;
    }
    return;
  }

  if (!ep->busy) {
    return;
  }

  if (ep->done < ep->len) {
    prv_tx_packet(m, n);
  } else {
    prv_done(m, n | PBL_UDC_EP_IN, ep, (int)ep->len);
  }
}

// A stall the endpoint sent interrupts too, without a packet
static void prv_rx_isr(const struct pbl_udc_musb *m, uint8_t n) {
  uint16_t csr;

  prv_index(m, n);
  csr = prv_rw(m, MUSB_RXCSR);
  if ((csr & MUSB_RXCSR_SENTSTALL) != 0U) {
    prv_ww(m, MUSB_RXCSR, csr & ~MUSB_RXCSR_SENTSTALL);
  }

  if ((csr & MUSB_RXCSR_RXPKTRDY) != 0U && m->state->out[n].busy) {
    prv_rx_packet(m, n);
  }
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
    if ((tx & PBL_BIT(n)) != 0U) {
      prv_tx_isr(m, n);
    }
    if ((rx & PBL_BIT(n)) != 0U) {
      prv_rx_isr(m, n);
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

// The FIFO of an endpoint number is the size of its packets, rounded up to a power of two, twice
// that for a bulk OUT endpoint if room is left, and stays its own until no endpoint is enabled, or
// one the number serves next needs more. An IN endpoint gains nothing from a second packet: the
// interrupt that frees the first comes as the host takes it, and the next one is in the FIFO long
// before the host asks again.
static int prv_fifo_alloc(const struct pbl_udc_musb *m, uint8_t n, uint16_t mps, bool two) {
  struct pbl_udc_musb_state *st = m->state;
  struct prv_fifo *fifo = &st->fifo[n];
  uint16_t room = m->fifo_size - st->fifo_end;
  uint16_t size = FIFO_MIN;

  while (size < mps) {
    size *= 2U;
  }

  if (fifo->size < size) {
    if (size > room) {
      return -ENOSPC;
    }

    fifo->addr = st->fifo_end;
    fifo->size = size;
    fifo->two = two && 2U * size + FIFO_RESERVE <= room;
    st->fifo_end += fifo->two ? 2U * size : size;
  }

  fifo->dpb = two && fifo->two;

  return 0;
}

static void prv_rx_dpb_set(const struct pbl_udc_musb *m) {
  uint16_t dis = (uint16_t)~PBL_BIT(0);

  for (uint8_t n = 1U; n < MAX_EPS; n++) {
    if (m->state->fifo[n].dpb) {
      dis &= ~PBL_BIT(n);
    }
  }

  prv_ww(m, MUSB_RXDPKTBUFDIS, dis);
}

static void prv_fifo_set(const struct pbl_udc_musb *m, uint8_t n) {
  const struct prv_fifo *fifo = &m->state->fifo[n];
  uint8_t sz = 0U;

  while ((FIFO_MIN << sz) < fifo->size) {
    sz++;
  }

  prv_wb(m, MUSB_TXFIFOSZ, sz);
  prv_ww(m, MUSB_TXFIFOADD, (uint16_t)(fifo->addr / 8U));
  prv_wb(m, MUSB_RXFIFOSZ, sz | (fifo->dpb ? MUSB_FIFOSZ_DPB : 0U));
  prv_ww(m, MUSB_RXFIFOADD, (uint16_t)(fifo->addr / 8U));
  prv_rx_dpb_set(m);
}

static int prv_ep_enable(const struct pbl_udc *udc, uint8_t addr, enum pbl_udc_ep_type type,
                         uint16_t mps) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  bool iso = type == PBL_UDC_EP_ISOCHRONOUS;
  int ret;
  uint8_t index;

  if (n >= MAX_EPS || mps > EP_MPS_MAX) {
    return -EINVAL;
  }

  if (n == 0U) {
    (in ? &m->state->in[0] : &m->state->out[0])->mps = mps;
    return 0;
  }

  // Each endpoint number serves one direction at a time, the one TXCSR.MODE selects
  if ((in ? m->state->out[n].mps : m->state->in[n].mps) != 0U) {
    return -EBUSY;
  }

  index = prv_lock(m);
  ret = prv_fifo_alloc(m, n, mps, type == PBL_UDC_EP_BULK && !in);
  if (ret != 0) {
    prv_unlock(m, index);
    return ret;
  }

  if (in) {
    m->state->in[n].mps = mps;
  } else {
    m->state->out[n].mps = mps;
  }

  prv_index(m, n);
  prv_fifo_set(m, n);
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
  prv_unlock(m, index);

  return 0;
}

// A flush per packet the FIFO holds, two in a double-buffered one
static void prv_rx_flush(const struct pbl_udc_musb *m) {
  for (uint8_t i = 0U; i < 2U; i++) {
    uint16_t csr = prv_rw(m, MUSB_RXCSR);

    if ((csr & MUSB_RXCSR_RXPKTRDY) == 0U) {
      return;
    }
    prv_ww(m, MUSB_RXCSR, csr | MUSB_RXCSR_FLUSHFIFO);
  }
}

static bool prv_eps_disabled(const struct pbl_udc_musb *m) {
  for (uint8_t n = 1U; n < MAX_EPS; n++) {
    if (m->state->in[n].mps != 0U || m->state->out[n].mps != 0U) {
      return false;
    }
  }

  return true;
}

static int prv_ep_disable(const struct pbl_udc *udc, uint8_t addr) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  struct prv_ep *ep = in ? &m->state->in[n] : &m->state->out[n];
  uint8_t index;

  index = prv_lock(m);
  if (n != 0U) {
    prv_index(m, n);
    if (in) {
      prv_ww(m, MUSB_INTRTXE, prv_rw(m, MUSB_INTRTXE) & ~PBL_BIT(n));
      prv_ww(m, MUSB_TXCSR, MUSB_TXCSR_MODE | MUSB_TXCSR_FLUSHFIFO);
    } else {
      prv_ww(m, MUSB_INTRRXE, prv_rw(m, MUSB_INTRRXE) & ~PBL_BIT(n));
      prv_rx_flush(m);
    }
  }
  if (ep->busy) {
    prv_done(m, addr, ep, -ECONNABORTED);
  }
  if (n != 0U) {
    ep->mps = 0U;
    if (prv_eps_disabled(m)) {
      prv_fifo_reset(m);
    }
  }
  prv_unlock(m, index);

  return 0;
}

static int prv_ep_set_halt(const struct pbl_udc *udc, uint8_t addr, bool halt) {
  const struct pbl_udc_musb *m = prv_musb(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  uint8_t index;

  index = prv_lock(m);
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
    if (!halt && m->state->in[n].busy && m->state->in[n].flushed) {
      m->state->in[n].flushed = false;
      prv_tx_packet(m, n);
    }
  } else {
    uint16_t csr = prv_rw(m, MUSB_RXCSR) & ~(MUSB_RXCSR_SENDSTALL | MUSB_RXCSR_SENTSTALL);

    prv_ww(m, MUSB_RXCSR, halt ? csr | MUSB_RXCSR_SENDSTALL : csr | MUSB_RXCSR_CLRDATATOG);
  }
  prv_unlock(m, index);

  return 0;
}

// Endpoint 0 follows the stage the stack is at: a data stage starts once the setup packet is
// served, and a status stage the controller already runs completes with it
static int prv_ep0_queue(const struct pbl_udc_musb *m, bool in, struct prv_ep *ep) {
  struct pbl_udc_musb_state *st = m->state;

  prv_index(m, 0U);
  switch (st->ep0) {
    case EP0_SETUP:
      // An IN request always has an IN data stage for the controller: one with no data, or none
      // at all, gets a zero-length packet
      if (ep->len == 0U && (st->setup[0] & SETUP_DIR_IN) == 0U) {
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
  uint8_t index;

  if (n >= MAX_EPS || ep->mps == 0U) {
    return -EINVAL;
  }

  index = prv_lock(m);
  if (ep->busy) {
    ret = -EBUSY;
  } else {
    ep->buf = buf;
    ep->len = len;
    ep->done = 0U;
    ep->busy = true;
    ep->flushed = false;

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
  prv_unlock(m, index);

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

  // One packet per FIFO: no double buffering
  prv_ww(m, MUSB_RXDPKTBUFDIS, (uint16_t)~PBL_BIT(0));
  prv_ww(m, MUSB_TXDPKTBUFDIS, (uint16_t)~PBL_BIT(0));
  prv_fifo_reset(m);

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

#define PBL_UDC_MUSB_DEFINE(sym, _name, _base, _irq, _quirks, _clk, _pinctrl, _syscfg, _fifo, ...) \
  PBL_UDC_STATE_DEFINE(sym);                                                                       \
  static struct pbl_udc_musb_state sym##_musb_state;                                               \
  PBL_CLK_ASSIGNED_DEFINE(sym##_assigned, __VA_ARGS__);                                            \
  static const struct pbl_pinctrl sym##_musb_pins = PBL_PINCTRL(_pinctrl, nullptr, 0);             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_udc_musb, struct pbl_udc, udc){                              \
    .udc = PBL_UDC_INIT(sym, _name, nullptr, &s_ops, SIFLI_NUM_EPS, PBL_UDC_SPEED_FULL,            \
                        PBL_UDC_CAP_EP_ONE_DIR),                                                   \
    .base = (_base),                                                                               \
    .irq = (_irq),                                                                                 \
    .quirks = (_quirks),                                                                           \
    .clk = _clk,                                                                                   \
    .assigned = sym##_assigned,                                                                    \
    .pinctrl = &sym##_musb_pins,                                                                   \
    .syscfg = (_syscfg),                                                                           \
    .fifo_size = (_fifo),                                                                          \
    .state = &sym##_musb_state,                                                                    \
  };                                                                                               \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UDC_MUSB_IRQ_PRIO, prv_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(sifli_sf32lb57_usb)
