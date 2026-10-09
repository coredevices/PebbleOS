# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(input.c)
pbl_library_sources_ifdef(CONFIG_INPUT_SHELL shell.c)
pbl_linker_sources(rom-start input.ld)
