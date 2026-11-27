# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(usbd_defines
  CONFIG_USB_DEVICE_VID=0x1209
  CONFIG_USB_DEVICE_PID=0x0001
  "CONFIG_USB_DEVICE_MANUFACTURER=\"PebbleOS\""
  "CONFIG_USB_DEVICE_PRODUCT=\"PebbleOS device\""
  CONFIG_USB_DEVICE_THREAD_PRIO=4
  CONFIG_USB_DEVICE_THREAD_STACK_SIZE=2048
)

set(usbd_bos_defines
  ${usbd_defines}
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
    ${usbd_bos_defines}
)

pbl_clar_test(test_usbd_bos_single
  SOURCES
    subsys/usb/usbd.c
    subsys/usb/bos.c
  DEFINES
    ${usbd_bos_defines}
)

pbl_clar_test(test_usbd_numbering
  SOURCES
    subsys/usb/usbd.c
  DEFINES
    ${usbd_defines}
)

pbl_clar_test(test_usbd_numbering_two_dir
  SOURCES
    subsys/usb/usbd.c
  DEFINES
    ${usbd_defines}
)

pbl_clar_test(test_usbd_numbering_overflow
  SOURCES
    subsys/usb/usbd.c
  DEFINES
    ${usbd_defines}
)

pbl_clar_test(test_usbd_other_speed
  SOURCES
    subsys/usb/usbd.c
  DEFINES
    ${usbd_defines}
)

pbl_clar_test(test_usbd_cdc_acm
  SOURCES
    subsys/usb/usbd.c
    subsys/usb/cdc_acm.c
    drivers/uart/uart.c
  DEFINES
    ${usbd_defines}
    CONFIG_USB_CDC_ACM_COUNT=2
    CONFIG_USB_CDC_ACM_TX_BUF_SIZE=1024
    "CONFIG_USB_CDC_ACM_NAME=\"Serial port\""
)
