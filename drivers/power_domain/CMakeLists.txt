# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources power_domain.c)
if(CONFIG_POWER_DOMAIN_SF32LB52_PMUC)
  list(APPEND sources sf32lb52_pmuc.c)
endif()
pbl_library_sources(${sources})
