# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources regulator.c)
if(CONFIG_REGULATOR_FIXED)
  list(APPEND sources fixed.c)
endif()
if(CONFIG_REGULATOR_NPM1300)
  list(APPEND sources npm1300.c)
endif()
if(CONFIG_REGULATOR_SF32LB52_PMUC)
  list(APPEND sources sf32lb52_pmuc.c)
endif()
pbl_library_sources(${sources})
