# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(hwspinlock.c)
pbl_library_sources_ifdef(CONFIG_HWSPINLOCK_STM32_HSEM stm32_hsem.c)
