# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources reset.c)
if(CONFIG_RESET_SF32LB)
  list(APPEND sources sf32lb.c)
endif()
pbl_library_sources(${sources})
