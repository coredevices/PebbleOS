# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(mipi_dsi.c)
pbl_library_sources_ifdef(CONFIG_MIPI_DSI_STM32 stm32.c)
