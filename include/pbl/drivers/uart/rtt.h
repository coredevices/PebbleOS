/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/drivers/uart.h>

/**
 * @defgroup drivers_uart_rtt SEGGER RTT
 * @ingroup drivers_uart
 * @brief @ref drivers_uart port over a channel of the @ref subsys_rtt control block.
 *
 * A serial port that a debug probe reads and writes over SWD while the target runs: it needs no
 * pins, and sends from the first instruction and from the fault handler. It uses up and down
 * channel @c CONFIG_UART_RTT_CHANNEL.
 *
 * Bytes sent while the channel is full are dropped, so the port never blocks without a probe.
 * Received bytes are polled every @c CONFIG_UART_RTT_POLL_MS while reception is enabled, which
 * keeps the system out of its deepest idle states meanwhile. A flush from an interrupt, as the
 * fault handler's before a reboot, waits up to @c CONFIG_UART_RTT_FLUSH_TIMEOUT_MS for a probe
 * seen reading to empty the channel. Line settings have no effect.
 * @{
 */

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_uart pbl_uart_rtt;
/** @endcond */

/** @brief The RTT port. */
#define RTT_UART (&pbl_uart_rtt)

/** @} */
