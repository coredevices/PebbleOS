/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "usbd_private.h"

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/drivers/mcu.h>
#include <pbl/drivers/udc.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#define EP0_MPS     64U
#define EP0_BUF_LEN 512U

#ifdef CONFIG_USB_DEVICE_BOS
#define BCD_USB 0x0210U
#else
#define BCD_USB 0x0200U
#endif

#define USB_CFG_VALUE 1U

#define STRING_LANGID       0U
#define STRING_MANUFACTURER 1U
#define STRING_PRODUCT      2U
#if defined(CONFIG_USB_DEVICE_SERIAL_NUMBER)
#define STRING_SERIAL    3U
#define STRING_FUNCTIONS 4U
#else
#define STRING_FUNCTIONS 3U
#endif

#define SERIAL_ID_MAX 16U
// Hosts name serial ports after 12-character serial numbers, not after 16-character ones
#define SERIAL_LEN 6U

#define MAX_IFACES    16U
#define MAX_FUNCTIONS 8U
#define MAX_EPS       16U

// An endpoint the controller does not have, for one the stack did not give
#define EP_NONE 0xffU

// Bus-powered, at most 100 mA
#define USB_CFG_ATTR_ONE           0x80U
#define USB_CFG_ATTR_REMOTE_WAKEUP 0x20U
#define USB_CFG_MAX_POWER          50U

#define STATUS_REMOTE_WAKEUP 0x02U

#ifdef CONFIG_USB_DEVICE_REMOTE_WAKEUP
#define REMOTE_WAKEUP true
#else
#define REMOTE_WAKEUP false
#endif

#define BULK_MPS_FS 64U
#define BULK_MPS_HS 512U

// The two transfers of a control transfer stage, a bus reset with its two events, a setup packet
// and room for suspend and resume
#define EVENTS 16U

#define RECONNECT_MS 10U

PBL_LOG_MODULE_DEFINE(usbd, CONFIG_USB_DEVICE_LOG_LEVEL);

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_usbd_function, usbd_functions);
PBL_ITERABLE_SECTION_DECLARE(const struct pbl_usbd_listener, usbd_listeners);

enum prv_kind {
  KIND_EVENT,
  KIND_XFER_DONE,
  KIND_CONNECT,
};

struct pbl_usbd__event {
  uint8_t kind;
  // Bus event, or endpoint
  uint8_t id;
  // Bytes moved or a negative errno, or the sequence number of a setup packet
  int32_t val;
  uint8_t setup[8];
};

enum prv_stage {
  STAGE_IDLE,
  STAGE_DATA_IN,
  STAGE_DATA_OUT,
  STAGE_STATUS,
};

static struct {
  struct pbl_usb_setup setup;
  // The setup packet served, by its sequence number
  uint32_t seq;
  enum prv_stage stage;
  bool zlp;
  uint8_t config;
  uint8_t alt[MAX_IFACES];
  // Endpoints the host halted, by bit: OUT ones from bit 0, IN ones from bit 16
  uint32_t halted;
  // The function an OUT data stage is for, and where its data goes
  const struct pbl_usbd_function *out_fn;
  uint8_t *out_data;
  uint8_t buf[EP0_BUF_LEN];
} s_ctrl;

// The numbers the stack gave the functions, by their index in the section: interfaces, and the
// function owning each endpoint of the controller, plus 1, with its own address for it
static struct {
  uint8_t first_iface[MAX_FUNCTIONS];
  uint8_t num_ifaces[MAX_FUNCTIONS];
  struct {
    uint8_t fn;
    uint8_t ep;
  } eps[2][MAX_EPS];
} s_num;

// Bus state, kept by the stack's thread. The bus idles from the connection until the host resets
// it: that is no suspend to report.
static struct {
  bool reset;
  bool connected;
  bool suspended;
  bool remote_wakeup;
} s_bus;

// Filled by the controller's interrupt, drained by the stack's thread
static struct {
  struct pbl_usbd__event ring[EVENTS];
  uint32_t head;
  uint32_t tail;
  uint32_t setup_seq;
  uint32_t lost;
} s_events;

static atomic_bool s_enabled;

// Endpoints whose transfer ends reach their function, by prv_ep_bit(): those of the configuration
// and alternate settings in use, until the stack starts disabling them
static atomic_uint_least32_t s_live;

static void prv_drain(struct pbl_work *work);

static PBL_WORKQ_DEFINE(s_workq);
static PBL_WORK_DEFINE(s_drain, prv_drain);
PBL_THREAD_STACK_DEFINE(s_stack, CONFIG_USB_DEVICE_THREAD_STACK_SIZE);
static struct pbl_thread s_thread;

static uint8_t prv_index(const struct pbl_usbd_function *fn) {
  return (uint8_t)(fn - PBL_ITERABLE_SECTION_START(usbd_functions));
}

uint8_t pbl_usbd__first_iface(const struct pbl_usbd_function *fn) {
  return s_num.first_iface[prv_index(fn)];
}

uint8_t pbl_usbd__num_ifaces(const struct pbl_usbd_function *fn) {
  return s_num.num_ifaces[prv_index(fn)];
}

uint8_t pbl_usbd_ep(const struct pbl_usbd_function *fn, uint8_t ep) {
  bool in = (ep & PBL_UDC_EP_IN) != 0U;
  uint8_t owner = prv_index(fn) + 1U;

  for (uint8_t n = 1U; n < MAX_EPS; n++) {
    if (s_num.eps[in][n].fn == owner && s_num.eps[in][n].ep == ep) {
      return in ? (n | PBL_UDC_EP_IN) : n;
    }
  }

  return EP_NONE;
}

#if defined(CONFIG_USB_DEVICE_SERIAL_NUMBER)
// The unique ID folded to SERIAL_LEN bytes, in hex, empty without one
static char s_serial[2U * SERIAL_LEN + 1U];
#endif

static uint16_t prv_bulk_mps(enum pbl_udc_speed speed) {
  return speed == PBL_UDC_SPEED_HIGH ? BULK_MPS_HS : BULK_MPS_FS;
}

static void prv_notify(enum pbl_usbd_event evt) {
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (fn->event != nullptr) {
      fn->event(evt);
    }
  }

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_listener, usbd_listeners, l) {
    l->event(evt);
  }
}

uint16_t pbl_usbd_bulk_mps(void) {
  return prv_bulk_mps(pbl_udc_speed(UDC));
}

// The descriptors give interrupt intervals in frames, which high speed counts in 2^(n-1)
// microframes
static uint8_t prv_interval(uint8_t frames, enum pbl_udc_speed speed) {
  uint8_t n = 1U;

  if (speed != PBL_UDC_SPEED_HIGH) {
    return frames;
  }

  while (n < 16U && (1U << (n - 1U)) < frames * 8U) {
    n++;
  }

  return n;
}

// The descriptor at offset i of a function's descriptors
static const struct pbl_usb_desc_header *prv_desc(const struct pbl_usbd_function *fn, uint16_t i) {
  return (const struct pbl_usb_desc_header *)((const uint8_t *)fn->desc + i);
}

// The first endpoint of the controller in the direction of a function's endpoint that is free,
// with its other direction too where the controller serves one per number
static int prv_assign_ep(uint8_t index, uint8_t ep) {
  bool in = (ep & PBL_UDC_EP_IN) != 0U;
  uint8_t num_eps = PBL_MIN(UDC->num_eps, MAX_EPS);
  const struct pbl_usbd_function *fn = &PBL_ITERABLE_SECTION_START(usbd_functions)[index];

  if ((ep & 0x70U) != 0U || (ep & 0x0fU) == 0U) {
    return -EINVAL;
  }

  if (pbl_usbd_ep(fn, ep) != EP_NONE) {
    return 0;
  }

  for (uint8_t n = 1U; n < num_eps; n++) {
    if (s_num.eps[in][n].fn != 0U) {
      continue;
    }
    if ((UDC->caps & PBL_UDC_CAP_EP_ONE_DIR) != 0U && s_num.eps[!in][n].fn != 0U) {
      continue;
    }

    s_num.eps[in][n].fn = index + 1U;
    s_num.eps[in][n].ep = ep;
    return 0;
  }

  return -ENOSPC;
}

// Interfaces in the order of the functions, endpoints from the first the controller has free
static int prv_assign(void) {
  uint8_t iface = 0U;
  uint8_t index = 0U;
  int ret;

  memset(&s_num, 0, sizeof(s_num));

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    uint8_t num_ifaces = 0U;

    if (index >= MAX_FUNCTIONS) {
      return -ENOSPC;
    }

    for (uint16_t i = 0U; i < fn->desc_len; i += prv_desc(fn, i)->bLength) {
      const struct pbl_usb_desc_header *d = prv_desc(fn, i);

      if (d->bDescriptorType == PBL_USB_DESC_INTERFACE) {
        const struct pbl_usb_desc_iface *id = (const struct pbl_usb_desc_iface *)d;

        num_ifaces = PBL_MAX(num_ifaces, (uint8_t)(id->bInterfaceNumber + 1U));
      } else if (d->bDescriptorType == PBL_USB_DESC_ENDPOINT) {
        ret = prv_assign_ep(index, ((const struct pbl_usb_desc_ep *)d)->bEndpointAddress);
        if (ret != 0) {
          return ret;
        }
      }
    }

    if (iface + num_ifaces > MAX_IFACES) {
      return -ENOSPC;
    }

    s_num.first_iface[index] = iface;
    s_num.num_ifaces[index] = num_ifaces;
    iface += num_ifaces;
    index++;
  }

  return 0;
}

// An endpoint descriptor of a function as the controller has it at a bus speed
static bool prv_endpoint(const struct pbl_usbd_function *fn, const struct pbl_usb_desc_header *hdr,
                         enum pbl_udc_speed speed, uint8_t *ep, enum pbl_udc_ep_type *type,
                         uint16_t *mps, uint8_t *interval) {
  const struct pbl_usb_desc_ep *d = (const struct pbl_usb_desc_ep *)hdr;

  if (hdr->bDescriptorType != PBL_USB_DESC_ENDPOINT) {
    return false;
  }

  *ep = pbl_usbd_ep(fn, d->bEndpointAddress);
  *type = (enum pbl_udc_ep_type)(d->bmAttributes & 0x3U);
  *mps = (*type == PBL_UDC_EP_BULK) ? prv_bulk_mps(speed) : pbl_le16_to_cpu(d->wMaxPacketSize);
  *interval = (*type == PBL_UDC_EP_INTERRUPT) ? prv_interval(d->bInterval, speed) : d->bInterval;

  return true;
}

// A string index of a function as the device has it
static uint8_t prv_string_index(const struct pbl_usbd_function *fn, uint8_t first, uint8_t index) {
  return (index != 0U && index <= fn->num_strings) ? (uint8_t)(first + index - 1U) : 0U;
}

// The configuration as it is at a bus speed, as a descriptor of the given type
static uint16_t prv_config_desc(uint8_t *out, uint16_t room, enum pbl_udc_speed speed,
                                uint8_t type) {
  struct pbl_usb_desc_config *cfg = (struct pbl_usb_desc_config *)out;
  uint16_t len = sizeof(*cfg);
  uint8_t ifaces = 0U;
  uint8_t first_string = STRING_FUNCTIONS;

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    uint8_t first_iface = pbl_usbd__first_iface(fn);

    if (len + fn->desc_len > room) {
      return 0U;
    }

    memcpy(&out[len], fn->desc, fn->desc_len);
    for (uint16_t i = 0U; i < fn->desc_len; i += prv_desc(fn, i)->bLength) {
      const struct pbl_usb_desc_header *d = prv_desc(fn, i);
      void *copy = &out[len + i];
      uint8_t ep;
      enum pbl_udc_ep_type ep_type;
      uint16_t mps;
      uint8_t interval;

      if (d->bDescriptorType == PBL_USB_DESC_INTERFACE) {
        struct pbl_usb_desc_iface *iface = copy;

        iface->bInterfaceNumber += first_iface;
        iface->iInterface = prv_string_index(fn, first_string, iface->iInterface);
      } else if (d->bDescriptorType == PBL_USB_DESC_IAD) {
        struct pbl_usb_desc_iad *iad = copy;

        iad->bFirstInterface += first_iface;
        iad->iFunction = prv_string_index(fn, first_string, iad->iFunction);
      } else if (prv_endpoint(fn, d, speed, &ep, &ep_type, &mps, &interval)) {
        struct pbl_usb_desc_ep *epd = copy;

        epd->bEndpointAddress = ep;
        epd->wMaxPacketSize = pbl_cpu_to_le16(mps);
        epd->bInterval = interval;
      } else if (fn->desc_fixup != nullptr) {
        fn->desc_fixup(copy, first_iface);
      }
    }
    len += fn->desc_len;
    ifaces += pbl_usbd__num_ifaces(fn);
    first_string += fn->num_strings;
  }

  *cfg = (struct pbl_usb_desc_config){
    .bLength = sizeof(*cfg),
    .bDescriptorType = type,
    .wTotalLength = pbl_cpu_to_le16(len),
    .bNumInterfaces = ifaces,
    .bConfigurationValue = USB_CFG_VALUE,
    .bmAttributes = USB_CFG_ATTR_ONE | (REMOTE_WAKEUP ? USB_CFG_ATTR_REMOTE_WAKEUP : 0U),
    .bMaxPower = USB_CFG_MAX_POWER,
  };

  return len;
}

// Miscellaneous, with interface association descriptors; iSerialNumber is set with the ID
static struct pbl_usb_desc_device s_device_desc = {
  .bLength = sizeof(struct pbl_usb_desc_device),
  .bDescriptorType = PBL_USB_DESC_DEVICE,
  .bcdUSB = PBL_CPU_TO_LE16(BCD_USB),
  .bDeviceClass = 0xefU,
  .bDeviceSubClass = 0x02U,
  .bDeviceProtocol = 0x01U,
  .bMaxPacketSize0 = EP0_MPS,
  .idVendor = PBL_CPU_TO_LE16(CONFIG_USB_DEVICE_VID),
  .idProduct = PBL_CPU_TO_LE16(CONFIG_USB_DEVICE_PID),
  .bcdDevice = PBL_CPU_TO_LE16(0x0100U),
  .iManufacturer = STRING_MANUFACTURER,
  .iProduct = STRING_PRODUCT,
  .bNumConfigurations = 1U,
};

// How the device would be at the other speed, which a high-speed device tells
static const struct pbl_usb_desc_qualifier s_qualifier_desc = {
  .bLength = sizeof(struct pbl_usb_desc_qualifier),
  .bDescriptorType = PBL_USB_DESC_DEVICE_QUALIFIER,
  .bcdUSB = PBL_CPU_TO_LE16(BCD_USB),
  .bDeviceClass = 0xefU,
  .bDeviceSubClass = 0x02U,
  .bDeviceProtocol = 0x01U,
  .bMaxPacketSize0 = EP0_MPS,
  .bNumConfigurations = 1U,
};

static const char *prv_string(uint8_t index) {
  if (index == STRING_MANUFACTURER) {
    return CONFIG_USB_DEVICE_MANUFACTURER;
  }
  if (index == STRING_PRODUCT) {
    return CONFIG_USB_DEVICE_PRODUCT;
  }
#if defined(CONFIG_USB_DEVICE_SERIAL_NUMBER)
  if (index == STRING_SERIAL) {
    return (s_serial[0] != '\0') ? s_serial : nullptr;
  }
#endif

  index -= STRING_FUNCTIONS;
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (index < fn->num_strings) {
      return fn->strings[index];
    }
    index -= fn->num_strings;
  }

  return nullptr;
}

static uint16_t prv_string_desc(uint8_t *out, uint16_t room, uint8_t index) {
  struct pbl_usb_desc_header *hdr = (struct pbl_usb_desc_header *)out;
  const char *str;
  uint16_t len;

  if (index == STRING_LANGID) {
    *hdr = (struct pbl_usb_desc_header){.bLength = 4U, .bDescriptorType = PBL_USB_DESC_STRING};
    pbl_put_le16(0x0409U, &out[2]);
    return 4U;
  }

  str = prv_string(index);
  if (str == nullptr) {
    return 0U;
  }

  len = (uint16_t)PBL_MIN(sizeof(*hdr) + 2U * strlen(str), room);
  *hdr =
      (struct pbl_usb_desc_header){.bLength = (uint8_t)len, .bDescriptorType = PBL_USB_DESC_STRING};
  for (uint16_t i = sizeof(*hdr); i + 1U < len; i += 2U) {
    out[i] = (uint8_t)str[(i - 2U) / 2U];
    out[i + 1U] = 0U;
  }

  return len;
}

static int prv_get_descriptor(uint8_t **data, uint16_t *len) {
  uint8_t type = (uint8_t)(s_ctrl.setup.wValue >> 8);
  uint8_t index = (uint8_t)s_ctrl.setup.wValue;
  enum pbl_udc_speed speed;

  *data = s_ctrl.buf;
  switch (type) {
    case PBL_USB_DESC_DEVICE:
      memcpy(s_ctrl.buf, &s_device_desc, sizeof(s_device_desc));
      *len = sizeof(s_device_desc);
      break;
    case PBL_USB_DESC_DEVICE_QUALIFIER:
      if (UDC->max_speed != PBL_UDC_SPEED_HIGH) {
        return -ENOTSUP;
      }
      memcpy(s_ctrl.buf, &s_qualifier_desc, sizeof(s_qualifier_desc));
      *len = sizeof(s_qualifier_desc);
      break;
    case PBL_USB_DESC_CONFIGURATION:
      if (index != 0U) {
        return -ENOTSUP;
      }
      *len = prv_config_desc(s_ctrl.buf, sizeof(s_ctrl.buf), pbl_udc_speed(UDC), type);
      break;
    case PBL_USB_DESC_OTHER_SPEED:
      if (UDC->max_speed != PBL_UDC_SPEED_HIGH || index != 0U) {
        return -ENOTSUP;
      }
      speed = pbl_udc_speed(UDC) == PBL_UDC_SPEED_HIGH ? PBL_UDC_SPEED_FULL : PBL_UDC_SPEED_HIGH;
      *len = prv_config_desc(s_ctrl.buf, sizeof(s_ctrl.buf), speed, type);
      break;
    case PBL_USB_DESC_STRING:
      *len = prv_string_desc(s_ctrl.buf, sizeof(s_ctrl.buf), index);
      break;
#ifdef CONFIG_USB_DEVICE_BOS
    case PBL_USB_DESC_BOS: {
      int ret = pbl_usbd__bos_desc(s_ctrl.buf, sizeof(s_ctrl.buf));

      if (ret < 0) {
        return ret;
      }
      *len = (uint16_t)ret;
      break;
    }
#endif
    default:
      return -ENOTSUP;
  }

  return *len != 0U ? 0 : -ENOTSUP;
}

static uint8_t prv_alt(uint8_t iface) {
  return iface < MAX_IFACES ? s_ctrl.alt[iface] : 0U;
}

static uint32_t prv_ep_bit(uint8_t ep) {
  return PBL_BIT((ep & 0x0fU) + (((ep & PBL_UDC_EP_IN) != 0U) ? 16U : 0U));
}

enum prv_eps_op {
  EPS_ON,
  EPS_CLOSE,
  EPS_OFF,
};

// Enables the endpoints of the current alternate setting of an interface of a function, by its own
// number, or of all its interfaces, stops handing it the ends of their transfers, or disables them
static int prv_ifaces_eps(const struct pbl_usbd_function *fn, int iface, enum prv_eps_op op) {
  int cur = -1;
  uint8_t alt = 0U;
  int ret;

  for (uint16_t i = 0U; i < fn->desc_len; i += prv_desc(fn, i)->bLength) {
    const struct pbl_usb_desc_header *d = prv_desc(fn, i);
    uint8_t ep;
    enum pbl_udc_ep_type type;
    uint16_t mps;
    uint8_t interval;

    if (d->bDescriptorType == PBL_USB_DESC_INTERFACE) {
      const struct pbl_usb_desc_iface *iface = (const struct pbl_usb_desc_iface *)d;

      cur = iface->bInterfaceNumber;
      alt = iface->bAlternateSetting;
      continue;
    }
    if (cur < 0 || (iface >= 0 && cur != iface) ||
        alt != prv_alt(pbl_usbd__first_iface(fn) + (uint8_t)cur)) {
      continue;
    }
    if (!prv_endpoint(fn, d, pbl_udc_speed(UDC), &ep, &type, &mps, &interval)) {
      continue;
    }

    if (op == EPS_CLOSE) {
      (void)atomic_fetch_and(&s_live, ~prv_ep_bit(ep));
      continue;
    }

    s_ctrl.halted &= ~prv_ep_bit(ep);
    if (op == EPS_OFF) {
      (void)pbl_udc_ep_disable(UDC, ep);
      continue;
    }

    ret = pbl_udc_ep_enable(UDC, ep, type, mps);
    if (ret != 0) {
      PBL_LOG_WRN("usb: endpoint 0x%02x of %u bytes failed (%d)", ep, mps, ret);
      return ret;
    }
    (void)atomic_fetch_or(&s_live, prv_ep_bit(ep));
  }

  return 0;
}

static int prv_ifaces_on(const struct pbl_usbd_function *fn, int iface) {
  return prv_ifaces_eps(fn, iface, EPS_ON);
}

// The interrupt may end a transfer between two disables: once one endpoint is off, none of the
// others hands its function a transfer end that could queue on it
static void prv_ifaces_off(const struct pbl_usbd_function *fn, int iface) {
  (void)prv_ifaces_eps(fn, iface, EPS_CLOSE);
  (void)prv_ifaces_eps(fn, iface, EPS_OFF);
}

static void prv_eps_off(void) {
  atomic_store(&s_live, 0U);
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    (void)prv_ifaces_eps(fn, -1, EPS_OFF);
  }
}

// Every interface is at its alternate setting 0 when not configured; a configuration whose
// endpoints the controller cannot all take is not set
static int prv_set_configured(bool on) {
  int ret;

  if (!on) {
    prv_eps_off();
  } else {
    PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
      ret = prv_ifaces_on(fn, -1);
      if (ret != 0) {
        prv_eps_off();
        return ret;
      }
    }
  }

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (fn->configured != nullptr) {
      fn->configured(on);
    }
  }

  memset(s_ctrl.alt, 0, sizeof(s_ctrl.alt));

  return 0;
}

// The function of an interface of the device, and its own number for it
static const struct pbl_usbd_function *prv_function(uint8_t iface, uint8_t *local) {
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    uint8_t first = pbl_usbd__first_iface(fn);

    if (iface >= first && iface < first + pbl_usbd__num_ifaces(fn)) {
      *local = iface - first;
      return fn;
    }
  }

  return nullptr;
}

static bool prv_alt_exists(const struct pbl_usbd_function *fn, uint8_t iface, uint8_t alt) {
  for (uint16_t i = 0U; i < fn->desc_len; i += prv_desc(fn, i)->bLength) {
    const struct pbl_usb_desc_iface *d = (const struct pbl_usb_desc_iface *)prv_desc(fn, i);

    if (d->bDescriptorType == PBL_USB_DESC_INTERFACE && d->bInterfaceNumber == iface &&
        d->bAlternateSetting == alt) {
      return true;
    }
  }

  return false;
}

static int prv_get_interface(uint16_t iface, uint8_t **data, uint16_t *len) {
  uint8_t local;

  if (s_ctrl.config == 0U || iface >= MAX_IFACES ||
      prv_function((uint8_t)iface, &local) == nullptr) {
    return -EINVAL;
  }

  s_ctrl.buf[0] = s_ctrl.alt[iface];
  *data = s_ctrl.buf;
  *len = 1U;

  return 0;
}

static int prv_set_interface(uint16_t iface, uint16_t alt) {
  const struct pbl_usbd_function *fn;
  uint8_t local;
  uint8_t old;
  int ret;

  if (s_ctrl.config == 0U || iface >= MAX_IFACES || alt > UINT8_MAX) {
    return -EINVAL;
  }

  fn = prv_function((uint8_t)iface, &local);
  if (fn == nullptr || !prv_alt_exists(fn, local, (uint8_t)alt)) {
    return -EINVAL;
  }

  old = s_ctrl.alt[iface];
  prv_ifaces_off(fn, local);
  s_ctrl.alt[iface] = (uint8_t)alt;
  ret = prv_ifaces_on(fn, local);
  if (ret != 0) {
    prv_ifaces_off(fn, local);
    s_ctrl.alt[iface] = old;
    (void)prv_ifaces_on(fn, local);
    return ret;
  }
  if (fn->set_alt != nullptr) {
    fn->set_alt(local, (uint8_t)alt);
  }

  return 0;
}

static int prv_set_halt(uint8_t ep, bool halt) {
  int ret;

  ret = pbl_udc_ep_set_halt(UDC, ep, halt);
  if (ret != 0) {
    return ret;
  }

  if (halt) {
    s_ctrl.halted |= prv_ep_bit(ep);
  } else {
    s_ctrl.halted &= ~prv_ep_bit(ep);
  }

  return 0;
}

// A bus reset or a newer setup packet since makes the address one the host no longer wants
static int prv_set_address(uint8_t addr) {
  int ret = -ECANCELED;

  pbl_irq_lock();
  if (s_ctrl.seq == s_events.setup_seq) {
    ret = pbl_udc_set_address(UDC, addr);
  }
  pbl_irq_unlock();

  return ret;
}

static int prv_standard(uint8_t **data, uint16_t *len) {
  const struct pbl_usb_setup *setup = &s_ctrl.setup;
  uint8_t recip = setup->bmRequestType & PBL_USB_RECIP_MASK;
  int ret;

  *len = 0U;
  switch (setup->bRequest) {
    case PBL_USB_REQ_GET_DESCRIPTOR:
      return prv_get_descriptor(data, len);
    case PBL_USB_REQ_SET_ADDRESS:
      return prv_set_address((uint8_t)setup->wValue);
    case PBL_USB_REQ_SET_CONFIGURATION:
      if (setup->wValue > USB_CFG_VALUE) {
        return -EINVAL;
      }
      if (s_ctrl.config != 0U) {
        s_ctrl.config = 0U;
        (void)prv_set_configured(false);
      }
      if (setup->wValue == 0U) {
        return 0;
      }
      ret = prv_set_configured(true);
      if (ret != 0) {
        return ret;
      }
      s_ctrl.config = (uint8_t)setup->wValue;
      return 0;
    case PBL_USB_REQ_GET_CONFIGURATION:
      s_ctrl.buf[0] = s_ctrl.config;
      *data = s_ctrl.buf;
      *len = 1U;
      return 0;
    case PBL_USB_REQ_GET_STATUS:
      if (recip == PBL_USB_RECIP_DEVICE) {
        s_ctrl.buf[0] = s_bus.remote_wakeup ? STATUS_REMOTE_WAKEUP : 0U;
      } else {
        s_ctrl.buf[0] = (recip == PBL_USB_RECIP_ENDPOINT &&
                         (s_ctrl.halted & prv_ep_bit((uint8_t)setup->wIndex)) != 0U)
                            ? 1U
                            : 0U;
      }
      s_ctrl.buf[1] = 0U;
      *data = s_ctrl.buf;
      *len = 2U;
      return 0;
    case PBL_USB_REQ_GET_INTERFACE:
      if (recip != PBL_USB_RECIP_INTERFACE) {
        return -ENOTSUP;
      }
      return prv_get_interface(setup->wIndex, data, len);
    case PBL_USB_REQ_SET_INTERFACE:
      if (recip != PBL_USB_RECIP_INTERFACE) {
        return -ENOTSUP;
      }
      return prv_set_interface(setup->wIndex, setup->wValue);
    case PBL_USB_REQ_CLEAR_FEATURE:
    case PBL_USB_REQ_SET_FEATURE:
      if (REMOTE_WAKEUP && recip == PBL_USB_RECIP_DEVICE &&
          setup->wValue == PBL_USB_FEATURE_DEVICE_REMOTE_WAKEUP) {
        s_bus.remote_wakeup = setup->bRequest == PBL_USB_REQ_SET_FEATURE;
        return 0;
      }
      if (recip != PBL_USB_RECIP_ENDPOINT || setup->wValue != PBL_USB_FEATURE_ENDPOINT_HALT) {
        return -ENOTSUP;
      }
      return prv_set_halt((uint8_t)setup->wIndex, setup->bRequest == PBL_USB_REQ_SET_FEATURE);
    default:
      return -ENOTSUP;
  }
}

// A class or vendor request to an interface, for its function
static int prv_function_request(const struct pbl_usbd_function **fn, uint8_t **data,
                                uint16_t *len) {
  struct pbl_usb_setup *setup = &s_ctrl.setup;
  uint8_t local;

  if ((setup->bmRequestType & PBL_USB_RECIP_MASK) != PBL_USB_RECIP_INTERFACE) {
    return -ENOTSUP;
  }

  *fn = prv_function((uint8_t)setup->wIndex, &local);
  if (*fn == nullptr || (*fn)->class_request == nullptr) {
    return -ENOTSUP;
  }

  setup->wIndex = (setup->wIndex & 0xff00U) | local;

  return (*fn)->class_request(setup, data, len);
}

static int prv_device_vendor(uint8_t **data, uint16_t *len) {
#ifdef CONFIG_USB_DEVICE_BOS
  int ret = pbl_usbd__bos_vendor(&s_ctrl.setup, s_ctrl.buf, sizeof(s_ctrl.buf));

  if (ret < 0) {
    return ret;
  }
  *data = s_ctrl.buf;
  *len = (uint16_t)ret;

  return 0;
#else
  return -ENOTSUP;
#endif
}

// Endpoint 0 serves the setup packet the stack has, unless a newer one arrived since
static void prv_ep0_queue(uint8_t ep, void *buf, size_t len) {
  pbl_irq_lock();
  if (s_ctrl.seq == s_events.setup_seq) {
    (void)pbl_udc_ep_queue(UDC, ep, buf, len);
  }
  pbl_irq_unlock();
}

static void prv_stall(void) {
  pbl_irq_lock();
  if (s_ctrl.seq == s_events.setup_seq) {
    (void)pbl_udc_ep_set_halt(UDC, PBL_UDC_EP_IN, true);
    (void)pbl_udc_ep_set_halt(UDC, 0U, true);
  }
  pbl_irq_unlock();
  s_ctrl.stage = STAGE_IDLE;
}

static void prv_setup(const uint8_t *packet) {
  const struct pbl_usb_setup *setup = &s_ctrl.setup;
  const struct pbl_usbd_function *fn = nullptr;
  bool in;
  uint8_t *data = nullptr;
  uint16_t len = 0U;
  int ret;

  s_ctrl.setup = (struct pbl_usb_setup){
    .bmRequestType = packet[0],
    .bRequest = packet[1],
    .wValue = pbl_get_le16(&packet[2]),
    .wIndex = pbl_get_le16(&packet[4]),
    .wLength = pbl_get_le16(&packet[6]),
  };
  in = (setup->bmRequestType & PBL_USB_DIR_IN) != 0U;

  switch (setup->bmRequestType & PBL_USB_TYPE_MASK) {
    case PBL_USB_TYPE_STANDARD:
      ret = prv_standard(&data, &len);
      break;
    case PBL_USB_TYPE_CLASS:
      ret = prv_function_request(&fn, &data, &len);
      break;
    case PBL_USB_TYPE_VENDOR:
      // To the device, a request of the device's own; to an interface, its function's
      if ((setup->bmRequestType & PBL_USB_RECIP_MASK) != PBL_USB_RECIP_DEVICE) {
        ret = prv_function_request(&fn, &data, &len);
        break;
      }
      ret = prv_device_vendor(&data, &len);
      break;
    default:
      ret = -ENOTSUP;
      break;
  }

  if (ret != 0) {
    prv_stall();
    return;
  }

  if (setup->wLength == 0U) {
    s_ctrl.stage = STAGE_STATUS;
    prv_ep0_queue(PBL_UDC_EP_IN, nullptr, 0U);
  } else if (in) {
    len = PBL_MIN(len, setup->wLength);
    s_ctrl.zlp = len != 0U && len < setup->wLength && (len % EP0_MPS) == 0U;
    s_ctrl.stage = STAGE_DATA_IN;
    prv_ep0_queue(PBL_UDC_EP_IN, data, len);
  } else if (data != nullptr && len >= setup->wLength) {
    s_ctrl.out_fn = fn;
    s_ctrl.out_data = data;
    s_ctrl.stage = STAGE_DATA_OUT;
    prv_ep0_queue(0U, data, setup->wLength);
  } else {
    prv_stall();
  }
}

static void prv_ep0_done(uint8_t ep, int ret) {
  if (ret < 0) {
    s_ctrl.stage = STAGE_IDLE;
    return;
  }

  switch (s_ctrl.stage) {
    case STAGE_DATA_IN:
      if (ep != PBL_UDC_EP_IN) {
        break;
      }
      if (s_ctrl.zlp) {
        s_ctrl.zlp = false;
        prv_ep0_queue(PBL_UDC_EP_IN, nullptr, 0U);
        break;
      }
      s_ctrl.stage = STAGE_STATUS;
      prv_ep0_queue(0U, nullptr, 0U);
      break;
    case STAGE_DATA_OUT:
      if (ep != 0U) {
        break;
      }
      if (s_ctrl.out_fn != nullptr && s_ctrl.out_fn->control_out != nullptr) {
        s_ctrl.out_fn->control_out(&s_ctrl.setup, s_ctrl.out_data, (uint16_t)ret);
      }
      s_ctrl.stage = STAGE_STATUS;
      prv_ep0_queue(PBL_UDC_EP_IN, nullptr, 0U);
      break;
    default:
      s_ctrl.stage = STAGE_IDLE;
      break;
  }
}

// The configuration ends with the bus, at a reset or a disconnection
static void prv_unconfigure(void) {
  if (s_ctrl.config != 0U) {
    s_ctrl.config = 0U;
    (void)prv_set_configured(false);
  }
  s_ctrl.stage = STAGE_IDLE;
  s_bus.suspended = false;
  s_bus.remote_wakeup = false;
}

static void prv_connect(void) {
  int ret;

  if (s_bus.connected || !atomic_load(&s_enabled) || !pbl_udc_vbus_present(UDC)) {
    return;
  }

  ret = pbl_udc_enable(UDC);
  if (ret != 0) {
    PBL_LOG_ERR("Cannot connect: %d", ret);
    return;
  }

  s_bus.connected = true;
  s_bus.reset = false;
  prv_notify(PBL_USBD_EVENT_CONNECTED);
}

static void prv_disconnect(void) {
  if (!s_bus.connected) {
    return;
  }

  s_bus.connected = false;
  prv_unconfigure();
  (void)pbl_udc_disable(UDC);
  prv_notify(PBL_USBD_EVENT_DISCONNECTED);
}

static void prv_event(const struct pbl_usbd__event *e) {
  switch (e->id) {
    case PBL_UDC_EVENT_RESET:
      prv_unconfigure();
      (void)pbl_udc_ep_enable(UDC, 0U, PBL_UDC_EP_CONTROL, EP0_MPS);
      (void)pbl_udc_ep_enable(UDC, PBL_UDC_EP_IN, PBL_UDC_EP_CONTROL, EP0_MPS);
      s_bus.reset = true;
      prv_notify(PBL_USBD_EVENT_RESET);
      break;
    case PBL_UDC_EVENT_SETUP:
      s_ctrl.seq = (uint32_t)e->val;
      prv_setup(e->setup);
      break;
    case PBL_UDC_EVENT_SUSPEND:
      if (s_bus.connected && s_bus.reset && !s_bus.suspended) {
        s_bus.suspended = true;
        prv_notify(PBL_USBD_EVENT_SUSPENDED);
      }
      break;
    case PBL_UDC_EVENT_RESUME:
      if (s_bus.suspended) {
        s_bus.suspended = false;
        prv_notify(PBL_USBD_EVENT_RESUMED);
      }
      break;
    case PBL_UDC_EVENT_VBUS_READY:
      prv_connect();
      break;
    case PBL_UDC_EVENT_VBUS_REMOVED:
      prv_disconnect();
      break;
    default:
      break;
  }
}

static void prv_data_done(uint8_t ep, int ret) {
  bool in = (ep & PBL_UDC_EP_IN) != 0U;
  uint8_t n = ep & ~PBL_UDC_EP_IN;
  const struct pbl_usbd_function *fn;

  if (n >= MAX_EPS || s_num.eps[in][n].fn == 0U || (atomic_load(&s_live) & prv_ep_bit(ep)) == 0U) {
    return;
  }

  fn = &PBL_ITERABLE_SECTION_START(usbd_functions)[s_num.eps[in][n].fn - 1U];
  if (fn->xfer_done != nullptr) {
    fn->xfer_done(s_num.eps[in][n].ep, ret);
  }
}

// From the controller's interrupt, or from the stack's thread when an endpoint call ends a transfer
static void prv_put(uint8_t kind, uint8_t id, int32_t val, const uint8_t *setup) {
  struct pbl_usbd__event *e;

  pbl_irq_lock();
  // A reset also supersedes the setup packets before it
  if (kind == KIND_EVENT && (id == PBL_UDC_EVENT_SETUP || id == PBL_UDC_EVENT_RESET)) {
    val = (int32_t)++s_events.setup_seq;
  }
  if (s_events.head - s_events.tail >= EVENTS) {
    s_events.lost++;
    pbl_irq_unlock();
    (void)pbl_work_submit(&s_workq, &s_drain);
    return;
  }

  e = &s_events.ring[s_events.head % EVENTS];
  e->kind = kind;
  e->id = id;
  e->val = val;
  if (setup != nullptr) {
    memcpy(e->setup, setup, sizeof(e->setup));
  }
  s_events.head++;
  pbl_irq_unlock();

  (void)pbl_work_submit(&s_workq, &s_drain);
}

static void prv_udc_event(const struct pbl_udc *udc, enum pbl_udc_event evt, const uint8_t *setup,
                          void *arg) {
  // The configuration is gone with a reset or USB power, before the thread disables its endpoints
  if (evt == PBL_UDC_EVENT_RESET || evt == PBL_UDC_EVENT_VBUS_REMOVED) {
    atomic_store(&s_live, 0U);
  }
  prv_put(KIND_EVENT, (uint8_t)evt, 0, evt == PBL_UDC_EVENT_SETUP ? setup : nullptr);
}

// Endpoint 0 is the control path, which goes through the thread; the other endpoints are handed
// over at once
static void prv_udc_xfer_done(const struct pbl_udc *udc, uint8_t ep, int ret, void *arg) {
  if ((ep & ~PBL_UDC_EP_IN) != 0U) {
    prv_data_done(ep, ret);
    return;
  }

  prv_put(KIND_XFER_DONE, ep, ret, nullptr);
}

static const struct pbl_udc_callbacks s_callbacks = {
  .event = prv_udc_event,
  .xfer_done = prv_udc_xfer_done,
};

// Events were lost: the device, its functions unconfigured, leaves the bus and comes back for the
// host to enumerate it again. The controller may stop its clock once disabled.
static void prv_reconnect(uint32_t lost) {
  PBL_LOG_ERR("%lu events lost, reconnecting", (unsigned long)lost);

  prv_disconnect();

  pbl_irq_lock();
  s_events.tail = s_events.head;
  pbl_irq_unlock();

  pbl_thread_sleep(PBL_MSEC(RECONNECT_MS));
  prv_connect();
}

static void prv_drain(struct pbl_work *work) {
  for (;;) {
    struct pbl_usbd__event e;
    bool superseded;
    uint32_t lost;

    pbl_irq_lock();
    lost = s_events.lost;
    s_events.lost = 0U;
    if (lost != 0U || s_events.tail == s_events.head) {
      pbl_irq_unlock();
      if (lost != 0U) {
        prv_reconnect(lost);
        continue;
      }
      return;
    }
    e = s_events.ring[s_events.tail % EVENTS];
    superseded = (uint32_t)e.val != s_events.setup_seq;
    s_events.tail++;
    pbl_irq_unlock();

    if (e.kind == KIND_EVENT) {
      // A host only sends a setup packet before the last one is served if it gave up on that one
      if (e.id != PBL_UDC_EVENT_SETUP || !superseded) {
        prv_event(&e);
      }
    } else if (e.kind == KIND_XFER_DONE) {
      prv_ep0_done(e.id, (int)e.val);
    } else {
      prv_connect();
    }
  }
}

static int prv_thread_start(void) {
  const struct pbl_thread_attr attr = {
    .name = "usbd",
    .entry = pbl_workq_thread,
    .arg = &s_workq,
    .prio = CONFIG_USB_DEVICE_THREAD_PRIO,
    .privileged = true,
    .stack = s_stack,
    .stack_size = sizeof(s_stack),
  };

  return pbl_thread_create(&s_thread, &attr);
}

static int prv_enable(void) {
  static bool s_started;
  int ret;

  ret = prv_assign();
  if (ret != 0) {
    return ret;
  }

  ret = pbl_device_init(&UDC->dev);
  if (ret != 0) {
    return ret;
  }

  if (!s_started) {
    ret = prv_thread_start();
    if (ret != 0) {
      return ret;
    }
    s_started = true;
  }

  pbl_udc_register(UDC, &s_callbacks, nullptr);
  prv_put(KIND_CONNECT, 0U, 0, nullptr);

  return 0;
}

#if defined(CONFIG_USB_DEVICE_SERIAL_NUMBER)
static void prv_serial_init(void) {
  static const char hex[] = "0123456789ABCDEF";
  uint8_t id[SERIAL_ID_MAX];
  uint8_t folded[SERIAL_LEN] = {0};
  size_t len = sizeof(id);

  if (mcu_get_serial(id, &len) != 0) {
    return;
  }

  for (size_t i = 0U; i < len; i++) {
    folded[i % SERIAL_LEN] ^= id[i];
  }

  for (size_t i = 0U; i < SERIAL_LEN; i++) {
    s_serial[2U * i] = hex[folded[i] >> 4];
    s_serial[2U * i + 1U] = hex[folded[i] & 0xfU];
  }

  s_device_desc.iSerialNumber = STRING_SERIAL;
}
#endif

int pbl_usbd_enable(void) {
  int ret;

  if (atomic_exchange(&s_enabled, true)) {
    return 0;
  }

#if defined(CONFIG_USB_DEVICE_SERIAL_NUMBER)
  prv_serial_init();
#endif

  ret = prv_enable();
  if (ret != 0) {
    memset(&s_num, 0, sizeof(s_num));
    atomic_store(&s_enabled, false);
  }

  return ret;
}

int pbl_usbd_wakeup(void) {
  if (!s_bus.remote_wakeup) {
    return -EACCES;
  }

  return pbl_udc_wakeup(UDC);
}
