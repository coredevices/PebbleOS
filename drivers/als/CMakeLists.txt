# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(als.c)
pbl_library_sources_ifdef(CONFIG_ALS_OPT3001 opt3001.c)
pbl_library_sources_ifdef(CONFIG_ALS_W1160 w1160.c)
pbl_library_sources_ifdef(CONFIG_ALS_SIM sim.c)
