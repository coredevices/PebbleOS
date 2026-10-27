# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(dma.c)
pbl_library_sources_ifdef(CONFIG_DMA_SF32LB52 sf32lb52.c)
