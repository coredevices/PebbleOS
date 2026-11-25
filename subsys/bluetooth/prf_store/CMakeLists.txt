# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_BT_PRF_STORE_LAYOUT_LEGACY legacy.c)
pbl_library_sources_ifdef(CONFIG_BT_PRF_STORE_LAYOUT_RECORDS records.c)
