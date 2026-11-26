# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_rtt
  SOURCES
    subsys/rtt/rtt.c
  DEFINES
    CONFIG_RTT_NUM_UP=2
    CONFIG_RTT_NUM_DOWN=1
    CONFIG_RTT_UP_SIZE=8
    CONFIG_RTT_DOWN_SIZE=8
)
