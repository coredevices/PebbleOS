# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_compositor
  SOURCES
    pebble/applib/graphics/gtypes.c
    pebble/services/compositor/compositor.c
  OVERRIDES dummy_board
)
