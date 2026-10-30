# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(fs.c)
pbl_library_sources_ifdef(CONFIG_SHELL shell.c)
pbl_library_sources_ifdef(CONFIG_FS_PFS
  pfs/flash_translation.c
  pfs/pfs.c
  pfs/pfs_debug_shell.c
  pfs/pfs_fs.c
)
