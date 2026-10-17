# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(vibe.c)
pbl_library_sources_ifdef(CONFIG_VIBE_AW86225 aw86225.c)
pbl_library_sources_ifdef(CONFIG_VIBE_AW8623X aw8623x.c)
pbl_library_sources_ifdef(CONFIG_VIBE_DRV2604 drv2604.c)
pbl_library_sources_ifdef(CONFIG_VIBE_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_VIBE_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_VIBE_SHELL shell.c)
