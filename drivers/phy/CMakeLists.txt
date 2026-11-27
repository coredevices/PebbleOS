# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(phy.c)
pbl_library_sources_ifdef(CONFIG_PHY_NRF54L_USBHS nrf54l_usbhs.c)
pbl_library_sources_ifdef(CONFIG_PHY_USB_NOP usb_nop.c)
