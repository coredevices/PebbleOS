# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(sprf.c)
pbl_library_sources_ifdef(CONFIG_RECOVERY_FW prf.c)
pbl_library_sources_ifndef(CONFIG_RECOVERY_FW settings.c)
