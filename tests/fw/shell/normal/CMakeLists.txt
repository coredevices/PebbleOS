# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_normal_system_app_state_machine
  SOURCES
    fw/shell/normal/system_app_state_machine.c
)

pbl_clar_test(test_app_idle_timeout
  SOURCES
    fw/shell/normal/app_idle_timeout.c
)

pbl_clar_test(test_battery_ui_fsm
  SOURCES
    fw/shell/normal/battery_ui_fsm.c
    fw/services/battery/voltage/battery_curve.c
  PLATFORMS obelix
  DEFINES CONFIG_BATTERY_WARNING_FIRST_PERCENT=0 CONFIG_BATTERY_WARNING_SECOND_PERCENT=0
  OVERRIDES dummy_board
)

pbl_clar_test(test_battery_ui_fsm_percent
  SOURCE test_battery_ui_fsm.c
  SOURCES
    fw/shell/normal/battery_ui_fsm.c
    fw/services/battery/voltage/battery_curve.c
  PLATFORMS obelix
  DEFINES CONFIG_BATTERY_WARNING_FIRST_PERCENT=12 CONFIG_BATTERY_WARNING_SECOND_PERCENT=8
  OVERRIDES dummy_board
)

pbl_clar_test(test_button_lock
  SOURCES
    fw/shell/normal/button_lock.c
  DEFINES CONFIG_TOUCH
)
