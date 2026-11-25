# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  service.c
  endpoint.c
)
pbl_library_sources_ifdef(CONFIG_BT_AMS_CLIENT ams.c ams_util.c)
