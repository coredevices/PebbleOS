# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(remoteproc.c)
pbl_library_sources_ifdef(CONFIG_REMOTEPROC_SF32LB sf32lb.c sf32lb_patch.c
  sf32lb52_rfcal.c)
pbl_library_sources_ifdef(CONFIG_REMOTEPROC_SHELL shell.c)
