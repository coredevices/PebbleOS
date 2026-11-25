/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/drivers/udc.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/bits.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#define EP0_MPS     64U
#define EP0_BUF_LEN 512U

#define USB_CFG_VALUE 1U

#define STRING_LANGID       0U
#define STRING_MANUFACTURER 1U
#define STRING_PRODUCT      2U
#define STRING_FUNCTIONS    3U

#define MAX_IFACES 16U

// Bus-powered, at most 100 mA
#define USB_CFG_ATTRIBUTES 0x80U
#define USB_CFG_MAX_POWER  50U

#define BULK_MPS_FS 64U
#define BULK_MPS_HS 512U

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_usbd_function, usbd_functions);

enum prv_stage {
  STAGE_IDLE,
  STAGE_DATA_IN,
  STAGE_DATA_OUT,
  STAGE_STATUS,
};

static struct {
  struct pbl_usb_setup setup;
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

static atomic_bool s_enabled;

uint16_t pbl_usbd_bulk_mps(void) {
  return pbl_udc_speed(UDC) == PBL_UDC_SPEED_HIGH ? BULK_MPS_HS : BULK_MPS_FS;
}

// The descriptors give interrupt intervals in frames, which high speed counts in 2^(n-1)
// microframes
static uint8_t prv_interval(uint8_t frames) {
  uint8_t n = 1U;

  if (pbl_udc_speed(UDC) != PBL_UDC_SPEED_HIGH) {
    return frames;
  }

  while (n < 16U && (1U << (n - 1U)) < frames * 8U) {
    n++;
  }

  return n;
}

// Endpoint descriptors of a function as the bus speed has them, at desc[i]
static bool prv_endpoint(const uint8_t *desc, uint8_t *ep, enum pbl_udc_ep_type *type,
                         uint16_t *mps, uint8_t *interval) {
  if (desc[1] != PBL_USB_DESC_ENDPOINT) {
    return false;
  }

  *ep = desc[2];
  *type = (enum pbl_udc_ep_type)(desc[3] & 0x3U);
  *mps = (*type == PBL_UDC_EP_BULK) ? pbl_usbd_bulk_mps() : pbl_get_le16(&desc[4]);
  *interval = (*type == PBL_UDC_EP_INTERRUPT) ? prv_interval(desc[6]) : desc[6];

  return true;
}

// A string index of a function as the device has it
static uint8_t prv_string_index(const struct pbl_usbd_function *fn, uint8_t first, uint8_t index) {
  return (index != 0U && index <= fn->num_strings) ? (uint8_t)(first + index - 1U) : 0U;
}

static uint16_t prv_config_desc(uint8_t *out, uint16_t room) {
  uint16_t len = 9U;
  uint8_t ifaces = 0U;
  uint8_t first_string = STRING_FUNCTIONS;

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (len + fn->desc_len > room) {
      return 0U;
    }

    memcpy(&out[len], fn->desc, fn->desc_len);
    for (uint16_t i = 0U; i < fn->desc_len; i += fn->desc[i]) {
      const uint8_t *d = &fn->desc[i];
      uint8_t ep;
      enum pbl_udc_ep_type type;
      uint16_t mps;
      uint8_t interval;

      if (d[1] == PBL_USB_DESC_INTERFACE) {
        out[len + i + 8U] = prv_string_index(fn, first_string, d[8]);
      } else if (d[1] == PBL_USB_DESC_IAD) {
        out[len + i + 7U] = prv_string_index(fn, first_string, d[7]);
      } else if (prv_endpoint(d, &ep, &type, &mps, &interval)) {
        pbl_put_le16(mps, &out[len + i + 4U]);
        out[len + i + 6U] = interval;
      }
    }
    len += fn->desc_len;
    ifaces += fn->num_ifaces;
    first_string += fn->num_strings;
  }

  out[0] = 9U;
  out[1] = PBL_USB_DESC_CONFIGURATION;
  pbl_put_le16(len, &out[2]);
  out[4] = ifaces;
  out[5] = USB_CFG_VALUE;
  out[6] = 0U;
  out[7] = USB_CFG_ATTRIBUTES;
  out[8] = USB_CFG_MAX_POWER;

  return len;
}

// The device, or how it would be at the other speed, which a high-speed device tells
static uint16_t prv_device_desc(uint8_t *out, bool qualifier) {
  uint16_t len = qualifier ? 10U : 18U;

  out[0] = (uint8_t)len;
  out[1] = qualifier ? PBL_USB_DESC_DEVICE_QUALIFIER : PBL_USB_DESC_DEVICE;
  pbl_put_le16(0x0200U, &out[2]);
  // Miscellaneous, with interface association descriptors
  out[4] = 0xefU;
  out[5] = 0x02U;
  out[6] = 0x01U;
  out[7] = EP0_MPS;
  if (qualifier) {
    out[8] = 1U;
    out[9] = 0U;
    return len;
  }

  pbl_put_le16(CONFIG_USB_DEVICE_VID, &out[8]);
  pbl_put_le16(CONFIG_USB_DEVICE_PID, &out[10]);
  pbl_put_le16(0x0100U, &out[12]);
  out[14] = STRING_MANUFACTURER;
  out[15] = STRING_PRODUCT;
  out[16] = 0U;
  out[17] = 1U;

  return len;
}

static const char *prv_string(uint8_t index) {
  if (index == STRING_MANUFACTURER) {
    return CONFIG_USB_DEVICE_MANUFACTURER;
  }
  if (index == STRING_PRODUCT) {
    return CONFIG_USB_DEVICE_PRODUCT;
  }

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
  const char *str;
  uint16_t len;

  if (index == STRING_LANGID) {
    out[0] = 4U;
    out[1] = PBL_USB_DESC_STRING;
    pbl_put_le16(0x0409U, &out[2]);
    return 4U;
  }

  str = prv_string(index);
  if (str == nullptr) {
    return 0U;
  }

  len = (uint16_t)PBL_MIN(2U + 2U * strlen(str), room);
  out[0] = (uint8_t)len;
  out[1] = PBL_USB_DESC_STRING;
  for (uint16_t i = 2U; i + 1U < len; i += 2U) {
    out[i] = (uint8_t)str[(i - 2U) / 2U];
    out[i + 1U] = 0U;
  }

  return len;
}

static int prv_get_descriptor(uint8_t **data, uint16_t *len) {
  uint8_t type = (uint8_t)(s_ctrl.setup.wValue >> 8);
  uint8_t index = (uint8_t)s_ctrl.setup.wValue;

  *data = s_ctrl.buf;
  switch (type) {
    case PBL_USB_DESC_DEVICE:
      *len = prv_device_desc(s_ctrl.buf, false);
      break;
    case PBL_USB_DESC_DEVICE_QUALIFIER:
      if (UDC->max_speed != PBL_UDC_SPEED_HIGH) {
        return -ENOTSUP;
      }
      *len = prv_device_desc(s_ctrl.buf, true);
      break;
    case PBL_USB_DESC_CONFIGURATION:
      *len = prv_config_desc(s_ctrl.buf, sizeof(s_ctrl.buf));
      break;
    case PBL_USB_DESC_STRING:
      *len = prv_string_desc(s_ctrl.buf, sizeof(s_ctrl.buf), index);
      break;
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

// Enables or disables the endpoints of the current alternate setting of an interface of a
// function, or of all its interfaces
static void prv_ifaces_switch(const struct pbl_usbd_function *fn, int iface, bool on) {
  int cur = -1;
  uint8_t alt = 0U;

  for (uint16_t i = 0U; i < fn->desc_len; i += fn->desc[i]) {
    const uint8_t *d = &fn->desc[i];
    uint8_t ep;
    enum pbl_udc_ep_type type;
    uint16_t mps;
    uint8_t interval;

    if (d[1] == PBL_USB_DESC_INTERFACE) {
      cur = d[2];
      alt = d[3];
      continue;
    }
    if (cur < 0 || (iface >= 0 && cur != iface) || alt != prv_alt((uint8_t)cur)) {
      continue;
    }
    if (!prv_endpoint(d, &ep, &type, &mps, &interval)) {
      continue;
    }

    if (on) {
      (void)pbl_udc_ep_enable(UDC, ep, type, mps);
    } else {
      (void)pbl_udc_ep_disable(UDC, ep);
    }
    s_ctrl.halted &= ~prv_ep_bit(ep);
  }
}

// Every interface is at its alternate setting 0 when not configured
static void prv_set_configured(bool on) {
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    prv_ifaces_switch(fn, -1, on);
    if (fn->configured != nullptr) {
      fn->configured(on);
    }
  }

  memset(s_ctrl.alt, 0, sizeof(s_ctrl.alt));
}

static const struct pbl_usbd_function *prv_function(uint8_t iface) {
  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (iface >= fn->first_iface && iface < fn->first_iface + fn->num_ifaces) {
      return fn;
    }
  }

  return nullptr;
}

static bool prv_alt_exists(const struct pbl_usbd_function *fn, uint8_t iface, uint8_t alt) {
  for (uint16_t i = 0U; i < fn->desc_len; i += fn->desc[i]) {
    const uint8_t *d = &fn->desc[i];

    if (d[1] == PBL_USB_DESC_INTERFACE && d[2] == iface && d[3] == alt) {
      return true;
    }
  }

  return false;
}

static int prv_get_interface(uint16_t iface, uint8_t **data, uint16_t *len) {
  if (s_ctrl.config == 0U || iface >= MAX_IFACES || prv_function((uint8_t)iface) == nullptr) {
    return -EINVAL;
  }

  s_ctrl.buf[0] = s_ctrl.alt[iface];
  *data = s_ctrl.buf;
  *len = 1U;

  return 0;
}

static int prv_set_interface(uint16_t iface, uint16_t alt) {
  const struct pbl_usbd_function *fn;

  if (s_ctrl.config == 0U || iface >= MAX_IFACES || alt > UINT8_MAX) {
    return -EINVAL;
  }

  fn = prv_function((uint8_t)iface);
  if (fn == nullptr || !prv_alt_exists(fn, (uint8_t)iface, (uint8_t)alt)) {
    return -EINVAL;
  }

  prv_ifaces_switch(fn, iface, false);
  s_ctrl.alt[iface] = (uint8_t)alt;
  prv_ifaces_switch(fn, iface, true);
  if (fn->set_alt != nullptr) {
    fn->set_alt((uint8_t)iface, (uint8_t)alt);
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

static int prv_standard(uint8_t **data, uint16_t *len) {
  const struct pbl_usb_setup *setup = &s_ctrl.setup;
  uint8_t recip = setup->bmRequestType & PBL_USB_RECIP_MASK;

  *len = 0U;
  switch (setup->bRequest) {
    case PBL_USB_REQ_GET_DESCRIPTOR:
      return prv_get_descriptor(data, len);
    case PBL_USB_REQ_SET_ADDRESS:
      return pbl_udc_set_address(UDC, (uint8_t)setup->wValue);
    case PBL_USB_REQ_SET_CONFIGURATION:
      if (setup->wValue > USB_CFG_VALUE) {
        return -EINVAL;
      }
      if (s_ctrl.config != 0U) {
        prv_set_configured(false);
      }
      s_ctrl.config = (uint8_t)setup->wValue;
      if (s_ctrl.config != 0U) {
        prv_set_configured(true);
      }
      return 0;
    case PBL_USB_REQ_GET_CONFIGURATION:
      s_ctrl.buf[0] = s_ctrl.config;
      *data = s_ctrl.buf;
      *len = 1U;
      return 0;
    case PBL_USB_REQ_GET_STATUS:
      s_ctrl.buf[0] = (recip == PBL_USB_RECIP_ENDPOINT &&
                       (s_ctrl.halted & prv_ep_bit((uint8_t)setup->wIndex)) != 0U)
                          ? 1U
                          : 0U;
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
  const struct pbl_usb_setup *setup = &s_ctrl.setup;

  if ((setup->bmRequestType & PBL_USB_RECIP_MASK) != PBL_USB_RECIP_INTERFACE) {
    return -ENOTSUP;
  }

  *fn = prv_function((uint8_t)setup->wIndex);
  if (*fn == nullptr || (*fn)->class_request == nullptr) {
    return -ENOTSUP;
  }

  return (*fn)->class_request(setup, data, len);
}

static void prv_stall(void) {
  (void)pbl_udc_ep_set_halt(UDC, PBL_UDC_EP_IN, true);
  (void)pbl_udc_ep_set_halt(UDC, 0U, true);
  s_ctrl.stage = STAGE_IDLE;
}

static void prv_setup(const uint8_t *packet) {
  const struct pbl_usb_setup *setup = &s_ctrl.setup;
  const struct pbl_usbd_function *fn = nullptr;
  bool in;
  uint8_t *data = nullptr;
  uint16_t len = 0U;
  int ret;

  memcpy(&s_ctrl.setup, packet, sizeof(s_ctrl.setup));
  in = (setup->bmRequestType & PBL_USB_DIR_IN) != 0U;

  switch (setup->bmRequestType & PBL_USB_TYPE_MASK) {
    case PBL_USB_TYPE_STANDARD:
      ret = prv_standard(&data, &len);
      break;
    case PBL_USB_TYPE_CLASS:
      ret = prv_function_request(&fn, &data, &len);
      break;
    case PBL_USB_TYPE_VENDOR:
      // To the device, a request of the device's own, of which there is none; to an interface, its
      // function's
      ret = ((setup->bmRequestType & PBL_USB_RECIP_MASK) == PBL_USB_RECIP_DEVICE)
                ? -ENOTSUP
                : prv_function_request(&fn, &data, &len);
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
    (void)pbl_udc_ep_queue(UDC, PBL_UDC_EP_IN, nullptr, 0U);
  } else if (in) {
    len = PBL_MIN(len, setup->wLength);
    s_ctrl.zlp = len != 0U && len < setup->wLength && (len % EP0_MPS) == 0U;
    s_ctrl.stage = STAGE_DATA_IN;
    (void)pbl_udc_ep_queue(UDC, PBL_UDC_EP_IN, data, len);
  } else if (data != nullptr && len >= setup->wLength) {
    s_ctrl.out_fn = fn;
    s_ctrl.out_data = data;
    s_ctrl.stage = STAGE_DATA_OUT;
    (void)pbl_udc_ep_queue(UDC, 0U, data, setup->wLength);
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
        (void)pbl_udc_ep_queue(UDC, PBL_UDC_EP_IN, nullptr, 0U);
        break;
      }
      s_ctrl.stage = STAGE_STATUS;
      (void)pbl_udc_ep_queue(UDC, 0U, nullptr, 0U);
      break;
    case STAGE_DATA_OUT:
      if (ep != 0U) {
        break;
      }
      if (s_ctrl.out_fn != nullptr && s_ctrl.out_fn->control_out != nullptr) {
        s_ctrl.out_fn->control_out(&s_ctrl.setup, s_ctrl.out_data, (uint16_t)ret);
      }
      s_ctrl.stage = STAGE_STATUS;
      (void)pbl_udc_ep_queue(UDC, PBL_UDC_EP_IN, nullptr, 0U);
      break;
    default:
      s_ctrl.stage = STAGE_IDLE;
      break;
  }
}

static void prv_event(const struct pbl_udc *udc, enum pbl_udc_event evt, const uint8_t *setup,
                      void *arg) {
  switch (evt) {
    case PBL_UDC_EVENT_RESET:
      if (s_ctrl.config != 0U) {
        s_ctrl.config = 0U;
        prv_set_configured(false);
      }
      s_ctrl.stage = STAGE_IDLE;
      (void)pbl_udc_ep_enable(udc, 0U, PBL_UDC_EP_CONTROL, EP0_MPS);
      (void)pbl_udc_ep_enable(udc, PBL_UDC_EP_IN, PBL_UDC_EP_CONTROL, EP0_MPS);
      break;
    case PBL_UDC_EVENT_SETUP:
      prv_setup(setup);
      break;
    default:
      break;
  }
}

static void prv_xfer_done(const struct pbl_udc *udc, uint8_t ep, int ret, void *arg) {
  if ((ep & ~PBL_UDC_EP_IN) == 0U) {
    prv_ep0_done(ep, ret);
    return;
  }

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    for (uint16_t i = 0U; i < fn->desc_len; i += fn->desc[i]) {
      if (fn->desc[i + 1U] == PBL_USB_DESC_ENDPOINT && fn->desc[i + 2U] == ep) {
        if (fn->xfer_done != nullptr) {
          fn->xfer_done(ep, ret);
        }
        return;
      }
    }
  }
}

static const struct pbl_udc_callbacks s_callbacks = {
  .event = prv_event,
  .xfer_done = prv_xfer_done,
};

int pbl_usbd_enable(void) {
  int ret;

  if (atomic_exchange(&s_enabled, true)) {
    return 0;
  }

  ret = pbl_device_init(&UDC->dev);
  if (ret == 0) {
    pbl_udc_register(UDC, &s_callbacks, nullptr);
    ret = pbl_udc_enable(UDC);
  }
  if (ret != 0) {
    atomic_store(&s_enabled, false);
  }

  return ret;
}
