# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_input
  SOURCES
    subsys/input/input.c
  DEFINES
    PBL_NO_LINKER_SCRIPT=1
)
