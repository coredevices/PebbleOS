# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(app_session.c)
pbl_library_sources_ifndef(CONFIG_RECOVERY_FW capabilities.c)
