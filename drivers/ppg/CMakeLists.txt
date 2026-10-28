# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(ppg.c)
pbl_library_sources_ifdef(CONFIG_PPG_GH3026 gh3026.c)
pbl_library_sources_ifdef(CONFIG_PPG_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_PPG_SHELL shell.c)
