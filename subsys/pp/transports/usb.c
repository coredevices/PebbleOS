/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>

#include <pbl/drivers/udc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/pp/transport.h>
#include <pbl/pp/transports/usb.h>
#include <pbl/usb/usb.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

PBL_LOG_MODULE_DECLARE(pp, CONFIG_PP_LOG_LEVEL);

#define IFACE  0U
#define EP_OUT 1U
#define EP_IN  (PBL_UDC_EP_IN | 2U)

#define STRING_NAME 1U

#define REQ_OPEN  0x01U
#define REQ_CLOSE 0x02U

// Whole packets of high speed, and whole data cache lines for DMA
#define TX_LEN 1024U
#define RX_LEN 1024U

static void prv_ctrl_work_cb(struct pbl_work *work);
static void prv_rx_work_cb(struct pbl_work *work);
static void prv_tx_work_cb(struct pbl_work *work);

static const struct PBL_PACKED {
  struct pbl_usb_desc_iface iface;
  struct pbl_usb_desc_ep out;
  struct pbl_usb_desc_ep in;
} s_desc = {
  .iface =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bNumEndpoints = 2U,
        .bInterfaceClass = PBL_PP_USB_CLASS,
        .bInterfaceSubClass = PBL_PP_USB_SUBCLASS,
        .bInterfaceProtocol = PBL_PP_USB_PROTOCOL,
        .iInterface = STRING_NAME,
      },
  .out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_OUT,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .in = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes = PBL_UDC_EP_BULK,
    .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
  },
};

static const char *const s_strings[] = {
  "Pebble protocol",
};

PBL_USBD_FUNCTION_DECLARE(pp);

static PBL_MUTEX_DEFINE(s_lock);

static struct {
  // Under s_lock
  struct pbl_pp_session *session;
  unsigned int session_gen;
  // Written from the controller's interrupt
  atomic_bool configured;
  atomic_bool want_open;
  atomic_uint open_gen;
  atomic_bool tx_busy;
  size_t tx_len;
  int rx_len;
  struct pbl_work ctrl_work;
  struct pbl_work rx_work;
  struct pbl_work tx_work;
  uint8_t tx_buf[TX_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
  uint8_t rx_buf[RX_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
} s_usb = {
  .ctrl_work = PBL_WORK_INITIALIZER(prv_ctrl_work_cb),
  .rx_work = PBL_WORK_INITIALIZER(prv_rx_work_cb),
  .tx_work = PBL_WORK_INITIALIZER(prv_tx_work_cb),
};

static uint8_t prv_ep(uint8_t ep) {
  return pbl_usbd_ep(PBL_USBD_FUNCTION(pp), ep);
}

static void prv_send_next(struct pbl_pp_session *session) {
  size_t len;

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (session != s_usb.session || !atomic_load(&s_usb.configured) || atomic_load(&s_usb.tx_busy)) {
    pbl_mutex_unlock(&s_lock);
    return;
  }

  // USB delivers what it takes: the bytes leave the queue once in the buffer
  len = pbl_pp_session_send_queue_copy(session, 0U, sizeof(s_usb.tx_buf), s_usb.tx_buf);
  if (len > 0U) {
    pbl_pp_session_send_queue_consume(session, len);
    s_usb.tx_len = len;
    atomic_store(&s_usb.tx_busy, true);
    if (pbl_udc_ep_queue(UDC, prv_ep(EP_IN), s_usb.tx_buf, len) != 0) {
      atomic_store(&s_usb.tx_busy, false);
      PBL_LOG_WRN("USB send failed, %u bytes lost", (unsigned int)len);
    }
  }

  pbl_mutex_unlock(&s_lock);
}

const struct pbl_pp_transport pbl_pp_usb_transport = {
  .name = "usb",
  .priority = 1U,
  .shared = true,
  .send_next = prv_send_next,
};

static void prv_ctrl_work_cb(struct pbl_work *work) {
  const bool want = atomic_load(&s_usb.want_open) && atomic_load(&s_usb.configured);
  const unsigned int gen = atomic_load(&s_usb.open_gen);

  pbl_mutex_lock(&s_lock, PBL_FOREVER);

  if (s_usb.session != nullptr && (!want || gen != s_usb.session_gen)) {
    pbl_pp_session_close(s_usb.session);
    s_usb.session = nullptr;
    PBL_LOG_DBG("USB session closed");
  }

  if (want && s_usb.session == nullptr) {
    s_usb.session = pbl_pp_session_open(&pbl_pp_usb_transport, PBL_PP_DESTINATION_HYBRID, nullptr);
    if (s_usb.session != nullptr) {
      s_usb.session_gen = gen;
      pbl_pp_session_set_capabilities(
          s_usb.session, PBL_PP_CAP_RUN_STATE | PBL_PP_CAP_INFINITE_LOG_DUMPING |
                             PBL_PP_CAP_VOICE_API | PBL_PP_CAP_APP_MESSAGE_8K |
                             PBL_PP_CAP_WEATHER_APP | PBL_PP_CAP_EXTENDED_NOTIFICATIONS);
      PBL_LOG_DBG("USB session opened");
    }
  }

  pbl_mutex_unlock(&s_lock);
}

static void prv_rx_start(void) {
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_OUT), s_usb.rx_buf, sizeof(s_usb.rx_buf));
}

static void prv_rx_work_cb(struct pbl_work *work) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_usb.session != nullptr && s_usb.rx_len > 0) {
    pbl_pp_session_receive(s_usb.session, s_usb.rx_buf, (size_t)s_usb.rx_len);
  }
  pbl_mutex_unlock(&s_lock);

  // The host's next packets wait, NAKed, until the buffer is free again
  if (atomic_load(&s_usb.configured)) {
    prv_rx_start();
  }
}

static void prv_tx_work_cb(struct pbl_work *work) {
  pbl_mutex_lock(&s_lock, PBL_FOREVER);
  if (s_usb.session != nullptr) {
    pbl_pp_session_send_next(s_usb.session);
  }
  pbl_mutex_unlock(&s_lock);
}

static void prv_configured(bool on) {
  atomic_store(&s_usb.configured, on);
  atomic_store(&s_usb.tx_busy, false);
  if (on) {
    prv_rx_start();
  } else {
    atomic_store(&s_usb.want_open, false);
  }
  (void)pbl_work_submit(&pbl_sys_workq, &s_usb.ctrl_work);
}

static int prv_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  if ((setup->bmRequestType & PBL_USB_TYPE_MASK) != PBL_USB_TYPE_VENDOR || setup->wLength != 0U) {
    return -EINVAL;
  }

  switch (setup->bRequest) {
    case REQ_OPEN:
      atomic_fetch_add(&s_usb.open_gen, 1U);
      atomic_store(&s_usb.want_open, true);
      break;
    case REQ_CLOSE:
      atomic_store(&s_usb.want_open, false);
      break;
    default:
      return -ENOTSUP;
  }

  (void)pbl_work_submit(&pbl_sys_workq, &s_usb.ctrl_work);

  return 0;
}

// A transfer that fills its packets ends with a zero-length one, for the host to take it now
static void prv_xfer_done(uint8_t ep, int ret) {
  if (ep == EP_OUT) {
    if (ret >= 0) {
      s_usb.rx_len = ret;
      (void)pbl_work_submit(&pbl_sys_workq, &s_usb.rx_work);
    }
    return;
  }

  if (ret >= 0 && s_usb.tx_len != 0U && (s_usb.tx_len % pbl_usbd_bulk_mps()) == 0U) {
    s_usb.tx_len = 0U;
    if (pbl_udc_ep_queue(UDC, prv_ep(EP_IN), nullptr, 0U) == 0) {
      return;
    }
  }

  atomic_store(&s_usb.tx_busy, false);
  if (ret >= 0) {
    (void)pbl_work_submit(&pbl_sys_workq_high, &s_usb.tx_work);
  }
}

PBL_USBD_FUNCTION_DEFINE(pp) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
  .strings = s_strings,
  .num_strings = PBL_ARRAY_SIZE(s_strings),
  .configured = prv_configured,
  .class_request = prv_request,
  .xfer_done = prv_xfer_done,
  .winusb_guid = "{5C9B2E61-7A43-4D0F-8E2B-3F6A1D9C0B47}",
};
