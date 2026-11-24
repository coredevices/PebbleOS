/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/clk.h>
#include <pbl/drivers/udc.h>
#include <pbl/kernel/barrier.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>

#include <hardware/nrf52_power.h>
#include <hardware/nrf52_usbd.h>

#define NUM_EPS 8U
#define EP_MPS  64U

#define DMA_TIMEOUT_US   100U
#define READY_TIMEOUT_US 10000U

// Errata 171 and 187: an undocumented block, written around ENABLE
#define ERRATA_BLOCK     0x4006EC00U
#define ERRATA_UNLOCK    0x9375U
#define ERRATA_171_REG   0x4006EC14U
#define ERRATA_171_VALUE 0xC0U
#define ERRATA_187_REG   0x4006ED14U
#define ERRATA_187_VALUE 0x3U

#define USB_REQ_SET_ADDRESS 5U

struct prv_ep {
  uint8_t *buf;
  size_t len;
  size_t done;
  // Bytes of the packet the hardware is sending
  size_t chunk;
  uint16_t mps;
  bool busy;
  // An OUT packet the hardware holds, NAKing the next ones, until a transfer takes it
  bool pending;
};

struct pbl_udc_nrf5_state {
  struct prv_ep in[NUM_EPS];
  struct prv_ep out[NUM_EPS];
  uint8_t setup[8];
  // The stack connected; the controller is on while VBUS is up, and on the bus once the
  // regulator is ready
  bool connect;
  bool enabled;
  bool running;
  // EasyDMA moves data through RAM, word aligned, and descriptors may sit in flash
  uint32_t dma_buf[EP_MPS / sizeof(uint32_t)];
};

struct pbl_udc_nrf5 {
  struct pbl_udc udc;
  uintptr_t base;
  pbl_irq_t irq;
  struct pbl_clk clk;
  uintptr_t power;
  pbl_irq_t power_irq;
  struct pbl_udc_nrf5_state *state;
};

static const struct pbl_udc_nrf5 *prv_nrf5(const struct pbl_udc *udc) {
  return PBL_CONTAINER_OF(udc, const struct pbl_udc_nrf5, udc);
}

static inline uint32_t prv_getreg(const struct pbl_udc_nrf5 *u, uint32_t offset) {
  return pbl_readl(u->base + offset);
}

static inline void prv_putreg(const struct pbl_udc_nrf5 *u, uint32_t offset, uint32_t value) {
  pbl_writel(value, u->base + offset);
}

// Clears an event, reading it back so that the interrupt line drops before returning
static void prv_clear(const struct pbl_udc_nrf5 *u, uint32_t event) {
  prv_putreg(u, event, 0U);
  (void)prv_getreg(u, event);
}

static bool prv_event(const struct pbl_udc_nrf5 *u, uint32_t event) {
  if (prv_getreg(u, event) == 0U) {
    return false;
  }
  prv_clear(u, event);

  return true;
}

static struct prv_ep *prv_ep(const struct pbl_udc_nrf5 *u, uint8_t addr) {
  uint8_t n = addr & ~PBL_UDC_EP_IN;

  return ((addr & PBL_UDC_EP_IN) != 0U) ? &u->state->in[n] : &u->state->out[n];
}

static void prv_done(const struct pbl_udc_nrf5 *u, uint8_t addr, int ret) {
  struct prv_ep *ep = prv_ep(u, addr);

  ep->busy = false;
  pbl_udc_xfer_done(&u->udc, addr, ret);
}

static void prv_abort_all(const struct pbl_udc_nrf5 *u, int ret) {
  for (uint8_t n = 0U; n < NUM_EPS; n++) {
    if (u->state->in[n].busy) {
      prv_done(u, n | PBL_UDC_EP_IN, ret);
    }
    if (u->state->out[n].busy) {
      prv_done(u, n, ret);
    }
    u->state->out[n].pending = false;
  }
}

// One EasyDMA transfer at a time (errata 199 needs its register set during it), run to its end
static int prv_dma(const struct pbl_udc_nrf5 *u, uint32_t task, uint32_t end) {
  int ret;

  prv_clear(u, end);
  prv_putreg(u, NRF52_USBD_ERRATA_199_OFFSET, USBD_ERRATA_199_DMA_ON);
  pbl_wmb();
  prv_putreg(u, task, 1U);
  ret = PBL_POLL_BUSY_US(prv_getreg(u, end) != 0U, 1U, DMA_TIMEOUT_US);
  pbl_rmb();
  prv_putreg(u, NRF52_USBD_ERRATA_199_OFFSET, 0U);
  prv_clear(u, end);

  return ret;
}

// The next packet of an IN transfer, a zero-length one when it has no bytes
static int prv_tx_packet(const struct pbl_udc_nrf5 *u, uint8_t n) {
  struct prv_ep *ep = &u->state->in[n];

  ep->chunk = PBL_MIN(ep->len - ep->done, (size_t)ep->mps);
  memcpy(u->state->dma_buf, &ep->buf[ep->done], ep->chunk);
  prv_putreg(u, NRF52_USBD_EPIN_PTR_OFFSET(n), (uint32_t)(uintptr_t)u->state->dma_buf);
  prv_putreg(u, NRF52_USBD_EPIN_MAXCNT_OFFSET(n), ep->chunk);

  return prv_dma(u, NRF52_USBD_TASKS_STARTEPIN_OFFSET(n), NRF52_USBD_EVENTS_ENDEPIN_OFFSET(n));
}

// The host took the packet: the next one, or the end of the transfer
static void prv_tx_acked(const struct pbl_udc_nrf5 *u, uint8_t n) {
  struct prv_ep *ep = &u->state->in[n];
  int ret;

  if (!ep->busy) {
    return;
  }

  ep->done += ep->chunk;
  if (ep->done >= ep->len) {
    prv_done(u, n | PBL_UDC_EP_IN, (int)ep->done);
    return;
  }

  ret = prv_tx_packet(u, n);
  if (ret != 0) {
    prv_done(u, n | PBL_UDC_EP_IN, ret);
  }
}

// The packet the hardware received: into the transfer, which ends with a short packet or full
static void prv_rx_packet(const struct pbl_udc_nrf5 *u, uint8_t n) {
  struct prv_ep *ep = &u->state->out[n];
  size_t len = prv_getreg(u, NRF52_USBD_SIZE_EPOUT_OFFSET(n));
  size_t room = ep->len - ep->done;
  int ret;

  ep->pending = false;
  prv_putreg(u, NRF52_USBD_EPOUT_PTR_OFFSET(n), (uint32_t)(uintptr_t)u->state->dma_buf);
  prv_putreg(u, NRF52_USBD_EPOUT_MAXCNT_OFFSET(n), len);
  ret = prv_dma(u, NRF52_USBD_TASKS_STARTEPOUT_OFFSET(n), NRF52_USBD_EVENTS_ENDEPOUT_OFFSET(n));
  if (ret != 0) {
    prv_done(u, n, ret);
    return;
  }

  memcpy(&ep->buf[ep->done], u->state->dma_buf, PBL_MIN(len, room));
  ep->done += PBL_MIN(len, room);

  if (len < ep->mps || ep->done >= ep->len) {
    prv_done(u, n, (int)ep->done);
  } else if (n == 0U) {
    prv_putreg(u, NRF52_USBD_TASKS_EP0RCVOUT_OFFSET, 1U);
  }
}

static bool prv_setup_is_set_address(const struct pbl_udc_nrf5 *u) {
  return u->state->setup[0] == 0U && u->state->setup[1] == USB_REQ_SET_ADDRESS;
}

static uint16_t prv_setup_wlength(const struct pbl_udc_nrf5 *u) {
  return (uint16_t)(u->state->setup[6] | (u->state->setup[7] << 8));
}

static void prv_ep0_setup(const struct pbl_udc_nrf5 *u) {
  struct pbl_udc_nrf5_state *st = u->state;

  // A new setup packet ends whatever control transfer was under way
  if (st->in[0].busy) {
    prv_done(u, PBL_UDC_EP_IN, -ECONNABORTED);
  }
  if (st->out[0].busy) {
    prv_done(u, 0U, -ECONNABORTED);
  }

  for (uint32_t i = 0U; i < sizeof(st->setup); i++) {
    st->setup[i] =
        (uint8_t)prv_getreg(u, NRF52_USBD_BMREQUESTTYPE_OFFSET + (i * NRF52_USBD_SETUP_STRIDE));
  }

  pbl_udc_event(&u->udc, PBL_UDC_EVENT_SETUP, st->setup);
}

static void prv_ep0_datadone(const struct pbl_udc_nrf5 *u) {
  if (u->state->in[0].busy) {
    prv_tx_acked(u, 0U);
  } else if (u->state->out[0].busy) {
    prv_rx_packet(u, 0U);
  }
}

static void prv_epdata(const struct pbl_udc_nrf5 *u) {
  uint32_t status = prv_getreg(u, NRF52_USBD_EPDATASTATUS_OFFSET);

  prv_putreg(u, NRF52_USBD_EPDATASTATUS_OFFSET, status);

  for (uint8_t n = 1U; n < NUM_EPS; n++) {
    if ((status & USBD_EPDATASTATUS_EPIN(n)) != 0U) {
      prv_tx_acked(u, n);
    }
    if ((status & USBD_EPDATASTATUS_EPOUT(n)) != 0U) {
      if (u->state->out[n].busy) {
        prv_rx_packet(u, n);
      } else {
        u->state->out[n].pending = true;
      }
    }
  }
}

static void prv_bus_reset(const struct pbl_udc_nrf5 *u) {
  prv_abort_all(u, -ECONNRESET);

  for (uint8_t n = 1U; n < NUM_EPS; n++) {
    u->state->in[n].mps = 0U;
    u->state->out[n].mps = 0U;
  }
  prv_putreg(u, NRF52_USBD_EPINEN_OFFSET, PBL_BIT(0));
  prv_putreg(u, NRF52_USBD_EPOUTEN_OFFSET, PBL_BIT(0));

  pbl_udc_event(&u->udc, PBL_UDC_EVENT_RESET, nullptr);
  pbl_udc_event(&u->udc, PBL_UDC_EVENT_ENUMERATED, nullptr);
}

static void prv_isr(const struct pbl_udc_nrf5 *u) {
  if (prv_event(u, NRF52_USBD_EVENTS_USBRESET_OFFSET)) {
    prv_bus_reset(u);
  }

  if (prv_event(u, NRF52_USBD_EVENTS_USBEVENT_OFFSET)) {
    uint32_t cause = prv_getreg(u, NRF52_USBD_EVENTCAUSE_OFFSET);

    prv_putreg(u, NRF52_USBD_EVENTCAUSE_OFFSET, cause);
    if ((cause & USBD_EVENTCAUSE_SUSPEND) != 0U) {
      pbl_udc_event(&u->udc, PBL_UDC_EVENT_SUSPEND, nullptr);
    }
    if ((cause & USBD_EVENTCAUSE_RESUME) != 0U) {
      pbl_udc_event(&u->udc, PBL_UDC_EVENT_RESUME, nullptr);
    }
  }

  if (prv_event(u, NRF52_USBD_EVENTS_EP0SETUP_OFFSET)) {
    prv_ep0_setup(u);
  }

  if (prv_event(u, NRF52_USBD_EVENTS_EP0DATADONE_OFFSET)) {
    prv_ep0_datadone(u);
  }

  if (prv_event(u, NRF52_USBD_EVENTS_EPDATA_OFFSET)) {
    prv_epdata(u);
  }
}

static void prv_errata_write(uintptr_t reg, uint32_t value) {
  if (pbl_readl(ERRATA_BLOCK) == 0U) {
    pbl_writel(ERRATA_UNLOCK, ERRATA_BLOCK);
    pbl_writel(value, reg);
    pbl_writel(ERRATA_UNLOCK, ERRATA_BLOCK);
  } else {
    pbl_writel(value, reg);
  }
}

// VBUS is up: the controller on, which starts the USB regulator
static int prv_power_on(const struct pbl_udc_nrf5 *u) {
  int ret;

  ret = pbl_clk_enable(&u->clk);
  if (ret != 0) {
    return ret;
  }

  prv_errata_write(ERRATA_187_REG, ERRATA_187_VALUE);
  prv_errata_write(ERRATA_171_REG, ERRATA_171_VALUE);

  prv_putreg(u, NRF52_USBD_ENABLE_OFFSET, 1U);
  ret =
      PBL_POLL_BUSY_US((prv_getreg(u, NRF52_USBD_EVENTCAUSE_OFFSET) & USBD_EVENTCAUSE_READY) != 0U,
                       10U, READY_TIMEOUT_US);
  prv_putreg(u, NRF52_USBD_EVENTCAUSE_OFFSET, USBD_EVENTCAUSE_READY);

  prv_errata_write(ERRATA_171_REG, 0U);
  prv_errata_write(ERRATA_187_REG, 0U);

  if (ret != 0) {
    prv_putreg(u, NRF52_USBD_ENABLE_OFFSET, 0U);
    (void)pbl_clk_disable(&u->clk);
    return ret;
  }

  u->state->enabled = true;

  return 0;
}

// The regulator is ready: the pull-up on D+ connects to the bus
static void prv_connect(const struct pbl_udc_nrf5 *u) {
  prv_putreg(u, NRF52_USBD_EPINEN_OFFSET, PBL_BIT(0));
  prv_putreg(u, NRF52_USBD_EPOUTEN_OFFSET, PBL_BIT(0));
  prv_putreg(u, NRF52_USBD_INTEN_OFFSET,
             USBD_INTEN_USBRESET | USBD_INTEN_EP0DATADONE | USBD_INTEN_USBEVENT |
                 USBD_INTEN_EP0SETUP | USBD_INTEN_EPDATA);
  prv_putreg(u, NRF52_USBD_USBPULLUP_OFFSET, 1U);
  u->state->running = true;
}

static void prv_power_off(const struct pbl_udc_nrf5 *u) {
  bool was_running = u->state->running;

  prv_putreg(u, NRF52_USBD_USBPULLUP_OFFSET, 0U);
  prv_putreg(u, NRF52_USBD_INTEN_OFFSET, 0U);
  prv_putreg(u, NRF52_USBD_ENABLE_OFFSET, 0U);
  (void)pbl_clk_disable(&u->clk);
  u->state->enabled = false;
  u->state->running = false;
  prv_abort_all(u, -ECONNRESET);

  if (was_running) {
    pbl_udc_event(&u->udc, PBL_UDC_EVENT_RESET, nullptr);
  }
}

// With the IRQ lock held: the controller follows VBUS, the regulator and the stack
static int prv_update(const struct pbl_udc_nrf5 *u) {
  uint32_t status = pbl_readl(u->power + NRF52_POWER_USBREGSTATUS_OFFSET);
  int ret;

  if (!u->state->connect || (status & POWER_USBREGSTATUS_VBUSDETECT) == 0U) {
    if (u->state->enabled) {
      prv_power_off(u);
    }
    return 0;
  }

  if (!u->state->enabled) {
    ret = prv_power_on(u);
    if (ret != 0) {
      return ret;
    }
  }

  if (!u->state->running && (status & POWER_USBREGSTATUS_OUTPUTRDY) != 0U) {
    prv_connect(u);
  }

  return 0;
}

static bool prv_power_event(const struct pbl_udc_nrf5 *u, uint32_t event) {
  if (pbl_readl(u->power + event) == 0U) {
    return false;
  }
  pbl_writel(0U, u->power + event);
  (void)pbl_readl(u->power + event);

  return true;
}

static void prv_power_isr(const struct pbl_udc_nrf5 *u) {
  (void)prv_power_event(u, NRF52_POWER_EVENTS_USBDETECTED_OFFSET);
  (void)prv_power_event(u, NRF52_POWER_EVENTS_USBREMOVED_OFFSET);
  (void)prv_power_event(u, NRF52_POWER_EVENTS_USBPWRRDY_OFFSET);

  (void)prv_update(u);
}

static enum pbl_udc_speed prv_speed(const struct pbl_udc *udc) {
  return PBL_UDC_SPEED_FULL;
}

// The controller answers SET_ADDRESS itself
static int prv_set_address(const struct pbl_udc *udc, uint8_t addr) {
  return 0;
}

static void prv_dtoggle_data0(const struct pbl_udc_nrf5 *u, uint8_t addr) {
  uint32_t sel = PBL_FIELD_PREP(USBD_DTOGGLE_EP_MASK, addr & ~PBL_UDC_EP_IN) |
                 (((addr & PBL_UDC_EP_IN) != 0U) ? USBD_DTOGGLE_IO_IN : 0U);

  prv_putreg(u, NRF52_USBD_DTOGGLE_OFFSET, sel);
  prv_putreg(u, NRF52_USBD_DTOGGLE_OFFSET,
             sel | PBL_FIELD_PREP(USBD_DTOGGLE_VALUE_MASK, USBD_DTOGGLE_VALUE_DATA0));
}

static int prv_ep_enable(const struct pbl_udc *udc, uint8_t addr, enum pbl_udc_ep_type type,
                         uint16_t mps) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;

  if (n >= NUM_EPS || mps > EP_MPS) {
    return -EINVAL;
  }

  // The isochronous endpoint is endpoint 8, with a buffer of its own
  if (type == PBL_UDC_EP_ISOCHRONOUS) {
    return -ENOTSUP;
  }

  prv_ep(u, addr)->mps = mps;
  if (n == 0U) {
    return 0;
  }

  pbl_irq_lock();
  if (in) {
    pbl_modifyl(0U, PBL_BIT(n), u->base + NRF52_USBD_EPINEN_OFFSET);
  } else {
    pbl_modifyl(0U, PBL_BIT(n), u->base + NRF52_USBD_EPOUTEN_OFFSET);
    // Writing its size lets the endpoint take a packet
    prv_putreg(u, NRF52_USBD_SIZE_EPOUT_OFFSET(n), 0U);
  }
  prv_dtoggle_data0(u, addr);
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_disable(const struct pbl_udc *udc, uint8_t addr) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  struct prv_ep *ep = prv_ep(u, addr);

  pbl_irq_lock();
  if (n != 0U) {
    pbl_modifyl(PBL_BIT(n), 0U,
                u->base + (((addr & PBL_UDC_EP_IN) != 0U) ? NRF52_USBD_EPINEN_OFFSET
                                                          : NRF52_USBD_EPOUTEN_OFFSET));
    ep->mps = 0U;
  }
  ep->pending = false;
  if (ep->busy) {
    prv_done(u, addr, -ECONNABORTED);
  }
  pbl_irq_unlock();

  return 0;
}

static int prv_ep_set_halt(const struct pbl_udc *udc, uint8_t addr, bool halt) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;

  pbl_irq_lock();
  if (n == 0U) {
    // A request the stack refuses: its data or status stage is stalled
    if (halt) {
      prv_putreg(u, NRF52_USBD_TASKS_EP0STALL_OFFSET, 1U);
    }
  } else {
    prv_putreg(u, NRF52_USBD_EPSTALL_OFFSET,
               PBL_FIELD_PREP(USBD_EPSTALL_EP_MASK, n) |
                   (((addr & PBL_UDC_EP_IN) != 0U) ? USBD_EPSTALL_IO_IN : 0U) |
                   (halt ? USBD_EPSTALL_STALL : 0U));
    if (!halt) {
      prv_dtoggle_data0(u, addr);
    }
  }
  pbl_irq_unlock();

  return 0;
}

// Endpoint 0 follows the request: a data stage moves packets, the status stage is the
// controller's once told to run it
static int prv_ep0_queue(const struct pbl_udc_nrf5 *u, bool in, struct prv_ep *ep) {
  bool data_in = (u->state->setup[0] & PBL_UDC_EP_IN) != 0U;
  bool status = prv_setup_wlength(u) == 0U || in != data_in;

  if (status) {
    if (ep->len != 0U) {
      return -EBUSY;
    }
    if (!prv_setup_is_set_address(u)) {
      prv_putreg(u, NRF52_USBD_TASKS_EP0STATUS_OFFSET, 1U);
    }
    prv_done(u, in ? PBL_UDC_EP_IN : 0U, 0);
    return 0;
  }

  if (in) {
    return prv_tx_packet(u, 0U);
  }

  prv_putreg(u, NRF52_USBD_TASKS_EP0RCVOUT_OFFSET, 1U);

  return 0;
}

static int prv_ep_queue(const struct pbl_udc *udc, uint8_t addr, void *buf, size_t len) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  uint8_t n = addr & ~PBL_UDC_EP_IN;
  bool in = (addr & PBL_UDC_EP_IN) != 0U;
  struct prv_ep *ep;
  int ret = 0;

  if (n >= NUM_EPS) {
    return -EINVAL;
  }

  ep = prv_ep(u, addr);
  if (ep->mps == 0U) {
    return -EINVAL;
  }

  pbl_irq_lock();
  if (!u->state->running) {
    ret = -ENOTCONN;
  } else if (ep->busy) {
    ret = -EBUSY;
  } else {
    ep->buf = buf;
    ep->len = len;
    ep->done = 0U;
    ep->chunk = 0U;
    ep->busy = true;

    if (n == 0U) {
      ret = prv_ep0_queue(u, in, ep);
    } else if (in) {
      ret = prv_tx_packet(u, n);
    } else if (ep->pending) {
      prv_rx_packet(u, n);
    }

    if (ret != 0) {
      ep->busy = false;
    }
  }
  pbl_irq_unlock();

  return ret;
}

static int prv_enable(const struct pbl_udc *udc) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  int ret;

  pbl_irq_lock();
  u->state->connect = true;
  ret = prv_update(u);
  pbl_irq_unlock();

  return ret;
}

static int prv_disable(const struct pbl_udc *udc) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  int ret;

  pbl_irq_lock();
  u->state->connect = false;
  ret = prv_update(u);
  pbl_irq_unlock();

  return ret;
}

static int prv_init(const struct pbl_udc *udc) {
  const struct pbl_udc_nrf5 *u = prv_nrf5(udc);
  const uint32_t usb_events =
      POWER_INTEN_USBDETECTED | POWER_INTEN_USBREMOVED | POWER_INTEN_USBPWRRDY;
  int ret;

  ret = pbl_device_init(&u->clk.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  prv_putreg(u, NRF52_USBD_INTEN_OFFSET, 0U);
  prv_putreg(u, NRF52_USBD_ENABLE_OFFSET, 0U);

  for (uint8_t n = 0U; n < NUM_EPS; n++) {
    u->state->in[n].mps = 0U;
    u->state->out[n].mps = 0U;
  }
  u->state->in[0].mps = EP_MPS;
  u->state->out[0].mps = EP_MPS;

  (void)prv_power_event(u, NRF52_POWER_EVENTS_USBDETECTED_OFFSET);
  (void)prv_power_event(u, NRF52_POWER_EVENTS_USBREMOVED_OFFSET);
  (void)prv_power_event(u, NRF52_POWER_EVENTS_USBPWRRDY_OFFSET);
  pbl_writel(usb_events, u->power + NRF52_POWER_INTENSET_OFFSET);

  pbl_irq_enable(u->irq);
  pbl_irq_enable(u->power_irq);

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

#define PBL_UDC_NRF5_DEFINE(sym, _name, _base, _irq, _clk, _power, _power_irq)         \
  PBL_UDC_STATE_DEFINE(sym);                                                           \
  static struct pbl_udc_nrf5_state sym##_nrf5_state;                                   \
  PBL_DEVICE_INSTANCE(sym, struct pbl_udc_nrf5, struct pbl_udc, udc){                  \
    .udc = PBL_UDC_INIT(sym, _name, nullptr, &s_ops, NUM_EPS, PBL_UDC_SPEED_FULL, 0U), \
    .base = (_base),                                                                   \
    .irq = (_irq),                                                                     \
    .clk = _clk,                                                                       \
    .power = (_power),                                                                 \
    .power_irq = (_power_irq),                                                         \
    .state = &sym##_nrf5_state,                                                        \
  };                                                                                   \
  PBL_IRQ_CONNECT_NUM(_irq, CONFIG_UDC_NRF5_IRQ_PRIO, prv_isr, &sym##_inst, 0);        \
  PBL_IRQ_CONNECT_NUM(_power_irq, CONFIG_UDC_NRF5_IRQ_PRIO, prv_power_isr, &sym##_inst, 0)

#include PBL_DT_INSTANCES(nordic_nrf52840_usbd)
