# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(hyperbus.c)
pbl_library_sources_ifdef(CONFIG_HYPERBUS_SF32LB52_MPI sf32lb52_mpi.c)
