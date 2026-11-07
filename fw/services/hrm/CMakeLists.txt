# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(hrm_agc.c hrm_manager.c hrm_ppg.c)
pbl_library_sources_ifdef(CONFIG_HRM_BACKEND_QEMU backend_qemu.c)
