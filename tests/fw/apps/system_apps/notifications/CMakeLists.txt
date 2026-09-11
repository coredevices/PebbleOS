# SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_notifications_history
  SOURCES
    src/fw/apps/system/notifications_history.c
  OVERRIDES dummy_board
)
