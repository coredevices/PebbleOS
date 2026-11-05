# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
set(sources pinctrl.c)
if(CONFIG_PINCTRL_SF32LB52)
  list(APPEND sources sf32lb52.c)
endif()
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_PINCTRL_SHELL shell.c)
