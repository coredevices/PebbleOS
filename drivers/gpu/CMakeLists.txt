# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(gpu.c)
pbl_library_sources_ifdef(CONFIG_GPU_SF32LB57_EPIC sf32lb57_epic.c)
