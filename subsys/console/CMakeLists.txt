# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  console.c
  frame.c
)
pbl_library_sources_ifdef(CONFIG_CONSOLE_IMAGING imaging.c)
pbl_library_sources_ifdef(CONFIG_SHELL shell.c)
