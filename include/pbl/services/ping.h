/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_ping Ping
 * @ingroup services
 * @brief Periodic ping to the phone, reporting whether the watch is idle.
 * @{
 */

/**
 * @brief Send a ping if one is due.
 *
 * Call it when other data is being sent to the phone anyway, to avoid waking the phone just for
 * the ping. Does nothing if a ping was sent less than an hour ago. The ping is sent from
 * KernelBG, so this may be called with the Bluetooth lock held.
 */
void ping_send_if_due(void);

/** @} */
