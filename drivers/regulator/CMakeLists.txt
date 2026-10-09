# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources regulator.c)
if(CONFIG_REGULATOR_NPM1300)
  list(APPEND sources npm1300.c)
endif()
pbl_library_sources(${sources})
