# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(remoteproc.c)
pbl_library_sources_ifdef(CONFIG_REMOTEPROC_NRF_VPR nrf_vpr.c)
pbl_library_sources_ifdef(CONFIG_REMOTEPROC_SF32LB sf32lb.c sf32lb_patch.c)
if(CONFIG_REMOTEPROC_SF32LB AND CONFIG_SOC_SF32LB52)
  pbl_library_sources(sf32lb52_rfcal.c)
endif()
if(CONFIG_REMOTEPROC_SF32LB AND CONFIG_SOC_SF32LB57)
  pbl_library_sources(sf32lb57_rfcal.c)
endif()
pbl_library_sources_ifdef(CONFIG_REMOTEPROC_STM32H7_CM4 stm32h7_cm4.c)
pbl_library_sources_ifdef(CONFIG_REMOTEPROC_SHELL shell.c)
