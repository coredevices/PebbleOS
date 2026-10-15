# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources clk.c)
if(CONFIG_CLK_SF32LB)
  list(APPEND sources sf32lb.c)
endif()
pbl_library_sources(${sources})
