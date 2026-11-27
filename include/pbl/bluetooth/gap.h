/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct ble_gap_event;

/**
 * @defgroup bluetooth_gap GAP
 * @ingroup bluetooth
 * @brief The handler of a connection's GAP events, and the size of device names.
 * @{
 */

/** @brief Buffer size of a device name, including the terminator. */
#define PBL_BT_DEVICE_NAME_BUFFER_SIZE (20)

/**
 * @brief Handle a GAP event of a connection.
 *
 * Connections are only established through advertising: pass this handler to
 * ble_gap_adv_start(). It decides on the pairing of the connections that leads to, and hands
 * their notifications to the GATT clients. Follow the other events with
 * ble_gap_event_listener_register().
 *
 * @param event Event.
 * @param arg Unused.
 * @return 0, or what the event asks for.
 */
int pbl_bt_gap_event_handler(struct ble_gap_event *event, void *arg);

/** @} */
