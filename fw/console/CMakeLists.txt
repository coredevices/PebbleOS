# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  console_frame.c
  console_imaging.c
  console_pp.c
  serial_console.c
)
pbl_library_sources_ifdef(CONFIG_SHELL console_shell.c)
