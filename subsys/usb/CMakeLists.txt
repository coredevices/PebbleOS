# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_iterable_section(usbd_functions)
pbl_iterable_section(usb_dfu_regions)
pbl_iterable_section(usb_msc_luns)
pbl_library_sources(usbd.c)
pbl_library_sources_ifdef(CONFIG_USB_DEVICE_BOS bos.c)
pbl_library_sources_ifdef(CONFIG_USB_CDC_ACM cdc_acm.c)
pbl_library_sources_ifdef(CONFIG_USB_DFU dfu.c)
pbl_library_sources_ifdef(CONFIG_USB_MSC msc.c)
pbl_library_sources_ifdef(CONFIG_USB_TEST test.c)
