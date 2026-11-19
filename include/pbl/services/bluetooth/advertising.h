/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_bluetooth_advertising Advertising
 * @ingroup services_bluetooth
 * @brief What the watch advertises, and how often.
 *
 * The watch advertises for one of two reasons, and airs the first that applies:
 *
 * | Reason | Payload | Short interval for |
 * | --- | --- | --- |
 * | Discovery | Flags, services, name, TX power; watch details in the scan response | 5 min |
 * | Reconnection | Empty | 30 s |
 *
 * It advertises every 20 ms at first and every 1022.5 ms after that, and not at all while
 * connected. A phone that keeps reconnecting and dropping gets the long interval right away.
 * @{
 */

/** @brief Start advertising what is asked for. Call once the stack runs. */
void bt_advertising_init(void);

/** @brief Stop advertising and forget what was asked for. Call before the stack stops. */
void bt_advertising_deinit(void);

/**
 * @brief Advertise for discovery, or stop.
 *
 * @param discoverable true to be discoverable; the short interval starts over.
 */
void bt_advertising_set_discoverable(bool discoverable);

/**
 * @brief Advertise for the bonded phone to reconnect, or stop.
 *
 * The recovery firmware never does, it stays discoverable instead.
 *
 * @param reconnectable true while there is a bond.
 */
void bt_advertising_set_reconnectable(bool reconnectable);

/** @brief Tell that a central connected: the stack stopped advertising. */
void bt_advertising_handle_connect(void);

/** @brief Tell that the central disconnected: advertising resumes. */
void bt_advertising_handle_disconnect(void);

/** @} */
