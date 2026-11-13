# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_kind(stlib)
pbl_library_sources(
  aes.c
  sha256.c
)

pbl_library_sources_ifdef(CONFIG_CRYPTO_SELFTEST selftest.c)

# Registers itself, so it cannot sit in the static library.
pbl_library_named(subsys__crypto__shell)
pbl_library_sources_ifdef(CONFIG_CRYPTO_SHELL shell.c)
