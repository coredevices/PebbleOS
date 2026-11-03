# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(mbox.c)
pbl_library_sources_ifdef(CONFIG_MBOX_SHELL shell.c)
