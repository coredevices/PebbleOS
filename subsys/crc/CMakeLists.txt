# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library_named(libcrc)
pbl_library_kind(stlib)
pbl_library_sources(
  crc8.c
  crc32.c
  crc32_legacy.c
)
