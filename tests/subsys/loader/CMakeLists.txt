# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_loader
  SOURCES
    subsys/loader/loader.c
  DEFINES
    CONFIG_LOADER_LOG_LEVEL=0
)
