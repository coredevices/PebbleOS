# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources clk.c)
if(CONFIG_CLK_SHELL)
  list(APPEND sources shell.c)
endif()
if(CONFIG_CLK_SF32LB)
  list(APPEND sources sf32lb.c)
endif()
if(CONFIG_CLK_CAL)
  list(APPEND sources cal.c)
endif()
if(CONFIG_CLK_SF32LB_RC10K)
  list(APPEND sources sf32lb_rc10k.c)
endif()
pbl_library_sources(${sources})
