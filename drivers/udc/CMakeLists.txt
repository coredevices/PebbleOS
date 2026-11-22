# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(udc.c)
pbl_library_sources_ifdef(CONFIG_UDC_DWC2 dwc2.c)
pbl_library_sources_ifdef(CONFIG_UDC_MUSB musb.c)
