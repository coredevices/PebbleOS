# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_uart_rtt
  SOURCES
    drivers/base/device.c
    drivers/uart/uart.c
    drivers/uart/rtt.c
    subsys/rtt/rtt.c
  DEFINES
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_RTT_NUM_UP=1
    CONFIG_RTT_NUM_DOWN=1
    CONFIG_RTT_UP_SIZE=8
    CONFIG_RTT_DOWN_SIZE=8
    CONFIG_UART_RTT_CHANNEL=0
    CONFIG_UART_RTT_POLL_MS=20
    CONFIG_UART_RTT_FLUSH_TIMEOUT_MS=1
)
