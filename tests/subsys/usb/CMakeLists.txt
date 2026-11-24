# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(usbd_defines
  CONFIG_USB_DEVICE_VID=0x1209
  CONFIG_USB_DEVICE_PID=0x0001
  "CONFIG_USB_DEVICE_MANUFACTURER=\"PebbleOS\""
  "CONFIG_USB_DEVICE_PRODUCT=\"PebbleOS device\""
  CONFIG_USB_DEVICE_BOS
  CONFIG_USB_DEVICE_MS_OS_20
  CONFIG_USB_DEVICE_WEBUSB
  "CONFIG_USB_DEVICE_WEBUSB_LANDING_PAGE=\"https://example.com/usb\""
)

pbl_clar_test(test_usbd_bos
  SOURCES
    subsys/usb/usbd.c
    subsys/usb/bos.c
  DEFINES
    ${usbd_defines}
)

pbl_clar_test(test_usbd_bos_single
  SOURCES
    subsys/usb/usbd.c
    subsys/usb/bos.c
  DEFINES
    ${usbd_defines}
)
