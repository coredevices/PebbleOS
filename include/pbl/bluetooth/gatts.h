/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/section.h>

/**
 * @defgroup bluetooth_gatts GATT server
 * @ingroup bluetooth
 * @brief What the GATT services of the device share.
 * @{
 */

struct ble_gatt_svc_def;

/** @brief A service of the device. Define it with PBL_BT_GATTS_SERVICE_DEFINE(). */
struct pbl_bt_gatts_service {
  /**
   * @brief Add the service to the GATT server, e.g. with pbl_bt_gatts_add().
   *
   * @return 0 on success, negative errno otherwise.
   */
  int (*add)(void);
};

/**
 * @brief Define a service of the device, added by pbl_bt_gatts_add_services().
 *
 * Services are added by increasing order, which sets their attribute handles. Peers cache the
 * handles of a bonded device, so the order of services that shipped must not change.
 *
 * @param _name Name of the service.
 * @param _order Two digits that place it among the others.
 * @param _add Its function, see struct pbl_bt_gatts_service.
 */
#define PBL_BT_GATTS_SERVICE_DEFINE(_name, _order, _add)                                      \
  static const struct pbl_bt_gatts_service pbl_bt_gatts_service_##_name PBL_ITERABLE_SECTION( \
      const struct pbl_bt_gatts_service, bt_gatts, #_order "_" #_name) = {.add = (_add)}

/**
 * @brief Add the services defined with PBL_BT_GATTS_SERVICE_DEFINE(), before the host starts.
 *
 * @return 0 on success, or what the first service that failed returned.
 */
int pbl_bt_gatts_add_services(void);

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
