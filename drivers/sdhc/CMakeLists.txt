# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(sdhc.c)
pbl_library_sources_ifdef(CONFIG_SDHC_SF32LB_SDMMC sf32lb_sdmmc.c)
pbl_library_sources_ifdef(CONFIG_SDHC_STM32_SDMMC stm32_sdmmc.c)
