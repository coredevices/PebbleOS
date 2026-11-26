/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup bluetooth_gatts GATT server
 * @ingroup bluetooth
 * @brief What the GATT services of the device share.
 * @{
 */

struct ble_gatt_svc_def;

/**
 * @brief Add services to the GATT server, before the host starts.
 *
 * @param svcs The services, ended by an empty one. They must outlive the server.
 * @return 0 on success, negative errno otherwise.
 */
int pbl_bt_gatts_add(const struct ble_gatt_svc_def *svcs);

/**
 * @brief Notify a value to a peer, subscribed or not.
 *
 * @param conn_handle Connection.
 * @param attr_handle Handle of the characteristic value.
 * @param data The value.
 * @param len Its length.
 * @return 0 on success, -ENOMEM if there is no buffer for it now, -ENOTCONN if the connection is
 * gone, another negative errno otherwise.
 */
int pbl_bt_gatts_notify(uint16_t conn_handle, uint16_t attr_handle, const void *data, uint16_t len);

/** @} */
