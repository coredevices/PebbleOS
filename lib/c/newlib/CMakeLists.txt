# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_LIBC_NEWLIB_NANO vsnprintf.c)
pbl_library_compile_options(-fno-builtin)
