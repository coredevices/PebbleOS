# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_interval_timer
  SOURCES
    fw/kernel/util/interval_timer.c
    tests/fakes/fake_rtc.c
)

pbl_clar_test(test_remote_input
  SOURCES
    fw/kernel/remote_input.c
  DEFINES CONFIG_SERVICE_TOUCH
)

pbl_clar_test(test_rand
  SOURCES
    fw/kernel/rand.c
    third_party/tinymt/TinyMT/tinymt/tinymt32.c
)
