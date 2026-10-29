# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_fs
  MODULES pfs
  SOURCES
    tests/fakes/fake_rtc.c
  PLATFORMS emery
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)
