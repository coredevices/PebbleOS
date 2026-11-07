/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup pp_transport_console Console transport
 * @ingroup pp_transport
 * @brief Pebble protocol over serial console frames, for development tools.
 * @{
 */

/**
 * @brief Handle a frame received on @c PBL_CONSOLE_CHANNEL_PP.
 *
 * Called by the console on SysWorkq.
 *
 * @param data Frame payload.
 * @param len Length of @p data in bytes.
 */
void pbl_pp_console_receive(const void *data, size_t len);

/**
 * @brief Handle the host's acknowledgment of a frame.
 *
 * Called by the console on the high priority work queue: senders on SysWorkq or KernelMain may
 * be blocked waiting for what this frees.
 *
 * @param seq Sequence number of the acknowledged frame.
 */
void pbl_pp_console_ack(uint8_t seq);

/** @} */
