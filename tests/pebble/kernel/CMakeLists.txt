# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_interval_timer
  SOURCES
    pebble/kernel/util/interval_timer.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_remote_input
  SOURCES
    pebble/kernel/remote_input.c
    pebble/kernel/input_buttons.c
    subsys/input/input.c
  DEFINES
    CONFIG_SERVICE_TOUCH
    PBL_NO_LINKER_SCRIPT=1
)

pbl_clar_test(test_rand
  MODULES rand
)
