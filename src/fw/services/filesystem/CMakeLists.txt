# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  app_file.c
  flash_translation.c
  pfs.c
)
pbl_library()
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_SHELL pfs_debug_shell.c)
