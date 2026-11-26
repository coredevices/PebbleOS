# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(reset.c)
pbl_library_sources_ifdef(CONFIG_RESET_SF32LB sf32lb.c)
pbl_library_sources_ifdef(CONFIG_RESET_STM32H7 stm32h7.c)
