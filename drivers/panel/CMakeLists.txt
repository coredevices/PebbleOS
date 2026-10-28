# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(panel.c)
pbl_library_sources_ifdef(CONFIG_PANEL_JDI_MIP jdi_mip.c)
