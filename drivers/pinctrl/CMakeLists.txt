# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(pinctrl.c)
pbl_library_sources_ifdef(CONFIG_PINCTRL_SHELL shell.c)
