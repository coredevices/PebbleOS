# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(services.c)

pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_BAS bas)
pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_DIS dis)
pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_PPOG_REVERSED ppog_reversed)
pbl_add_subdirectory_ifdef(CONFIG_BT_SERVICE_PPS pps)
