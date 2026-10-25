# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_quick_launch_slot
  SOURCES fw/apps/system/settings/quick_launch_slot.c
  PLATFORMS emery flint
  OVERRIDES dummy_board
)
