/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "usbd_private.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>

#define MS_OS_20_VENDOR_CODE 0x01U
#define MS_OS_20_DESC_INDEX  0x0007U
// Windows 8.1, the first with MS OS 2.0 descriptors
#define MS_OS_20_WIN_VERSION   0x06030000U
#define MS_OS_20_SET_HEADER    0x0000U
#define MS_OS_20_SUBSET_CONFIG 0x0001U
#define MS_OS_20_SUBSET_FUNC   0x0002U
#define MS_OS_20_COMPAT_ID     0x0003U
#define MS_OS_20_REG_PROPERTY  0x0004U
#define REG_MULTI_SZ           7U

// "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
#define GUID_LEN 38U

#define WEBUSB_VENDOR_CODE 0x02U
#define WEBUSB_GET_URL     0x0002U
#define WEBUSB_DESC_URL    0x03U
#define WEBUSB_LANDING     1U

struct PBL_PACKED pbl_usbd__ms_os_20_cap {
  struct pbl_usb_desc_platform platform;
  uint32_t dwWindowsVersion;
  uint16_t wMSOSDescriptorSetTotalLength;
  uint8_t bMS_VendorCode;
  uint8_t bAltEnumCode;
};

struct PBL_PACKED pbl_usbd__webusb_cap {
  struct pbl_usb_desc_platform platform;
  uint16_t bcdVersion;
  uint8_t bVendorCode;
  uint8_t iLandingPage;
};

struct PBL_PACKED pbl_usbd__ms_os_20_set_header {
  uint16_t wLength;
  uint16_t wDescriptorType;
  uint32_t dwWindowsVersion;
  uint16_t wTotalLength;
};

struct PBL_PACKED pbl_usbd__ms_os_20_subset_config {
  uint16_t wLength;
  uint16_t wDescriptorType;
  uint8_t bConfigurationValue;
  uint8_t bReserved;
  uint16_t wTotalLength;
};

struct PBL_PACKED pbl_usbd__ms_os_20_subset_func {
  uint16_t wLength;
  uint16_t wDescriptorType;
  uint8_t bFirstInterface;
  uint8_t bReserved;
  uint16_t wSubsetLength;
};

// The features of a function bound to WinUSB: its compatible ID, and its GUID as the only one of
// the DeviceInterfaceGUIDs registry property, a REG_MULTI_SZ string list in UTF-16LE
struct PBL_PACKED pbl_usbd__ms_os_20_winusb {
  uint16_t wLength;
  uint16_t wDescriptorType;
  uint8_t CompatibleID[8];
  uint8_t SubCompatibleID[8];
  struct PBL_PACKED {
    uint16_t wLength;
    uint16_t wDescriptorType;
    uint16_t wPropertyDataType;
    uint16_t wPropertyNameLength;
    uint8_t PropertyName[2U * sizeof("DeviceInterfaceGUIDs")];
    uint16_t wPropertyDataLength;
    uint8_t PropertyData[2U * (GUID_LEN + 2U)];
  } guids;
};

struct PBL_PACKED pbl_usbd__webusb_url {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bScheme;
  char URL[];
};

static_assert(sizeof(struct pbl_usbd__ms_os_20_cap) == 28U);
static_assert(sizeof(struct pbl_usbd__webusb_cap) == 24U);
static_assert(sizeof(struct pbl_usbd__ms_os_20_set_header) == 10U);
static_assert(sizeof(struct pbl_usbd__ms_os_20_subset_config) == 8U);
static_assert(sizeof(struct pbl_usbd__ms_os_20_subset_func) == 8U);
static_assert(sizeof(struct pbl_usbd__ms_os_20_winusb) == 152U);

PBL_ITERABLE_SECTION_DECLARE(const struct pbl_usbd_function, usbd_functions);

#ifdef CONFIG_USB_DEVICE_MS_OS_20
static const uint8_t s_ms_os_20_uuid[16] = {
  0xdf, 0x60, 0xdd, 0xd8, 0x89, 0x45, 0xc7, 0x4c, 0x9c, 0xd2, 0x65, 0x9d, 0x9e, 0x64, 0x8a, 0x9f,
};

// ASCII as UTF-16LE, into room bytes, the rest null characters
static void prv_utf16(uint8_t *out, size_t room, const char *str) {
  size_t len = strlen(str);

  memset(out, 0, room);
  for (size_t i = 0U; i < len && 2U * i < room; i++) {
    out[2U * i] = (uint8_t)str[i];
  }
}

static void prv_winusb(struct pbl_usbd__ms_os_20_winusb *out, const char *guid) {
  *out = (struct pbl_usbd__ms_os_20_winusb){
    .wLength = pbl_cpu_to_le16(offsetof(struct pbl_usbd__ms_os_20_winusb, guids)),
    .wDescriptorType = pbl_cpu_to_le16(MS_OS_20_COMPAT_ID),
    .CompatibleID = "WINUSB",
    .guids = {
      .wLength = pbl_cpu_to_le16(sizeof(out->guids)),
      .wDescriptorType = pbl_cpu_to_le16(MS_OS_20_REG_PROPERTY),
      .wPropertyDataType = pbl_cpu_to_le16(REG_MULTI_SZ),
      .wPropertyNameLength = pbl_cpu_to_le16(sizeof(out->guids.PropertyName)),
      .wPropertyDataLength = pbl_cpu_to_le16(sizeof(out->guids.PropertyData)),
    },
  };
  prv_utf16(out->guids.PropertyName, sizeof(out->guids.PropertyName), "DeviceInterfaceGUIDs");
  prv_utf16(out->guids.PropertyData, sizeof(out->guids.PropertyData), guid);
}

// The functions of a device with more than one interface go in function subsets
static bool prv_composite(void) {
  uint8_t ifaces = 0U;

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    ifaces += pbl_usbd__num_ifaces(fn);
  }

  return ifaces > 1U;
}

static uint16_t prv_function_len(bool composite) {
  return (composite ? sizeof(struct pbl_usbd__ms_os_20_subset_func) : 0U) +
         sizeof(struct pbl_usbd__ms_os_20_winusb);
}

// Length of the descriptor set, 0 when no function asks for WinUSB
static uint16_t prv_ms_os_20_len(bool composite) {
  uint16_t len = 0U;

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (fn->winusb_guid != nullptr) {
      len += prv_function_len(composite);
    }
  }

  if (len == 0U) {
    return 0U;
  }

  return len + sizeof(struct pbl_usbd__ms_os_20_set_header) +
         (composite ? sizeof(struct pbl_usbd__ms_os_20_subset_config) : 0U);
}

static int prv_ms_os_20_set(uint8_t *out, uint16_t room) {
  bool composite = prv_composite();
  uint16_t len = prv_ms_os_20_len(composite);
  uint8_t *p = out;

  if (len == 0U) {
    return -ENOTSUP;
  }
  if (len > room) {
    return -ENOSPC;
  }

  *(struct pbl_usbd__ms_os_20_set_header *)p = (struct pbl_usbd__ms_os_20_set_header){
    .wLength = pbl_cpu_to_le16(sizeof(struct pbl_usbd__ms_os_20_set_header)),
    .wDescriptorType = pbl_cpu_to_le16(MS_OS_20_SET_HEADER),
    .dwWindowsVersion = pbl_cpu_to_le32(MS_OS_20_WIN_VERSION),
    .wTotalLength = pbl_cpu_to_le16(len),
  };
  p += sizeof(struct pbl_usbd__ms_os_20_set_header);

  if (composite) {
    // Windows reads bConfigurationValue as the index of the configuration, not its value
    *(struct pbl_usbd__ms_os_20_subset_config *)p = (struct pbl_usbd__ms_os_20_subset_config){
      .wLength = pbl_cpu_to_le16(sizeof(struct pbl_usbd__ms_os_20_subset_config)),
      .wDescriptorType = pbl_cpu_to_le16(MS_OS_20_SUBSET_CONFIG),
      .wTotalLength = pbl_cpu_to_le16(len - sizeof(struct pbl_usbd__ms_os_20_set_header)),
    };
    p += sizeof(struct pbl_usbd__ms_os_20_subset_config);
  }

  PBL_ITERABLE_SECTION_FOREACH(const struct pbl_usbd_function, usbd_functions, fn) {
    if (fn->winusb_guid == nullptr) {
      continue;
    }
    if (strlen(fn->winusb_guid) != GUID_LEN) {
      return -EINVAL;
    }

    if (composite) {
      *(struct pbl_usbd__ms_os_20_subset_func *)p = (struct pbl_usbd__ms_os_20_subset_func){
        .wLength = pbl_cpu_to_le16(sizeof(struct pbl_usbd__ms_os_20_subset_func)),
        .wDescriptorType = pbl_cpu_to_le16(MS_OS_20_SUBSET_FUNC),
        .bFirstInterface = pbl_usbd__first_iface(fn),
        .wSubsetLength = pbl_cpu_to_le16(prv_function_len(true)),
      };
      p += sizeof(struct pbl_usbd__ms_os_20_subset_func);
    }
    prv_winusb((struct pbl_usbd__ms_os_20_winusb *)p, fn->winusb_guid);
    p += sizeof(struct pbl_usbd__ms_os_20_winusb);
  }

  return len;
}
#endif

#ifdef CONFIG_USB_DEVICE_WEBUSB
static const uint8_t s_webusb_uuid[16] = {
  0x38, 0xb6, 0x08, 0x34, 0xa9, 0x09, 0xa0, 0x47, 0x8b, 0xfd, 0xa0, 0x76, 0x88, 0x15, 0xb6, 0x65,
};

static int prv_webusb_url(uint8_t *out, uint16_t room, uint8_t index) {
  struct pbl_usbd__webusb_url *desc = (struct pbl_usbd__webusb_url *)out;
  const char *url = CONFIG_USB_DEVICE_WEBUSB_LANDING_PAGE;
  uint8_t scheme = 0xffU;
  size_t len;

  if (index != WEBUSB_LANDING || url[0] == '\0') {
    return -ENOTSUP;
  }

  if (strncmp(url, "http://", 7U) == 0) {
    scheme = 0U;
    url += 7U;
  } else if (strncmp(url, "https://", 8U) == 0) {
    scheme = 1U;
    url += 8U;
  }

  len = sizeof(*desc) + strlen(url);
  if (len > UINT8_MAX || len > room) {
    return -ENOSPC;
  }

  desc->bLength = (uint8_t)len;
  desc->bDescriptorType = WEBUSB_DESC_URL;
  desc->bScheme = scheme;
  memcpy(desc->URL, url, len - sizeof(*desc));

  return (int)len;
}
#endif

int pbl_usbd__bos_desc(uint8_t *out, uint16_t room) {
  struct pbl_usb_desc_bos *bos = (struct pbl_usb_desc_bos *)out;
  uint16_t len = sizeof(*bos);
  uint8_t caps = 0U;

  if (room < sizeof(*bos) + sizeof(struct pbl_usb_desc_usb20_ext) +
                 sizeof(struct pbl_usbd__ms_os_20_cap) + sizeof(struct pbl_usbd__webusb_cap)) {
    return -ENOSPC;
  }

  // No link power management
  *(struct pbl_usb_desc_usb20_ext *)&out[len] = (struct pbl_usb_desc_usb20_ext){
    .bLength = sizeof(struct pbl_usb_desc_usb20_ext),
    .bDescriptorType = PBL_USB_DESC_DEVICE_CAP,
    .bDevCapabilityType = PBL_USB_CAP_USB20_EXT,
  };
  len += sizeof(struct pbl_usb_desc_usb20_ext);
  caps++;

#ifdef CONFIG_USB_DEVICE_MS_OS_20
  uint16_t set_len = prv_ms_os_20_len(prv_composite());

  if (set_len != 0U) {
    struct pbl_usbd__ms_os_20_cap *cap = (struct pbl_usbd__ms_os_20_cap *)&out[len];

    *cap = (struct pbl_usbd__ms_os_20_cap){
      .platform =
          {
            .bLength = sizeof(*cap),
            .bDescriptorType = PBL_USB_DESC_DEVICE_CAP,
            .bDevCapabilityType = PBL_USB_CAP_PLATFORM,
          },
      .dwWindowsVersion = pbl_cpu_to_le32(MS_OS_20_WIN_VERSION),
      .wMSOSDescriptorSetTotalLength = pbl_cpu_to_le16(set_len),
      .bMS_VendorCode = MS_OS_20_VENDOR_CODE,
    };
    memcpy(cap->platform.PlatformCapabilityUUID, s_ms_os_20_uuid, sizeof(s_ms_os_20_uuid));
    len += sizeof(*cap);
    caps++;
  }
#endif

#ifdef CONFIG_USB_DEVICE_WEBUSB
  struct pbl_usbd__webusb_cap *webusb = (struct pbl_usbd__webusb_cap *)&out[len];

  *webusb = (struct pbl_usbd__webusb_cap){
    .platform =
        {
          .bLength = sizeof(*webusb),
          .bDescriptorType = PBL_USB_DESC_DEVICE_CAP,
          .bDevCapabilityType = PBL_USB_CAP_PLATFORM,
        },
    .bcdVersion = pbl_cpu_to_le16(0x0100U),
    .bVendorCode = WEBUSB_VENDOR_CODE,
    .iLandingPage = (CONFIG_USB_DEVICE_WEBUSB_LANDING_PAGE[0] != '\0') ? WEBUSB_LANDING : 0U,
  };
  memcpy(webusb->platform.PlatformCapabilityUUID, s_webusb_uuid, sizeof(s_webusb_uuid));
  len += sizeof(*webusb);
  caps++;
#endif

  *bos = (struct pbl_usb_desc_bos){
    .bLength = sizeof(*bos),
    .bDescriptorType = PBL_USB_DESC_BOS,
    .wTotalLength = pbl_cpu_to_le16(len),
    .bNumDeviceCaps = caps,
  };

  return len;
}

int pbl_usbd__bos_vendor(const struct pbl_usb_setup *setup, uint8_t *out, uint16_t room) {
  if (setup->bmRequestType != (PBL_USB_DIR_IN | PBL_USB_TYPE_VENDOR | PBL_USB_RECIP_DEVICE)) {
    return -ENOTSUP;
  }

#ifdef CONFIG_USB_DEVICE_MS_OS_20
  if (setup->bRequest == MS_OS_20_VENDOR_CODE && setup->wIndex == MS_OS_20_DESC_INDEX) {
    return prv_ms_os_20_set(out, room);
  }
#endif

#ifdef CONFIG_USB_DEVICE_WEBUSB
  if (setup->bRequest == WEBUSB_VENDOR_CODE && setup->wIndex == WEBUSB_GET_URL) {
    return prv_webusb_url(out, room, (uint8_t)setup->wValue);
  }
#endif

  return -ENOTSUP;
}
