/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/bluetooth/clients/gatt.h>

/**
 * @defgroup bluetooth_service_changed_client Service Changed client
 * @ingroup bluetooth
 * @brief Tells when the GATT services of a peer change.
 *
 * It subscribes to the Service Changed characteristic of the peer's GATT service. The clients of
 * the peer's other services then look for theirs again, e.g. with
 * pbl_bt_gattc_service_rediscover().
 * @{
 */

struct pbl_bt_service_changed_client;

/**
 * @brief The services of the peer changed. Called on the host task.
 *
 * @param client Client.
 * @param start_handle First attribute handle affected.
 * @param end_handle Last attribute handle affected.
 */
typedef void (*pbl_bt_service_changed_cb_t)(struct pbl_bt_service_changed_client *client,
                                            uint16_t start_handle, uint16_t end_handle);

/** @brief A client. Define it with PBL_BT_SERVICE_CHANGED_CLIENT_DEFINE(). */
struct pbl_bt_service_changed_client {
  /** @cond INTERNAL_HIDDEN */
  struct pbl_bt_gattc_service svc;
  struct pbl_bt_gattc_chr chrs[1];
  pbl_bt_service_changed_cb_t cb;
  /** @endcond */
};

/** @cond INTERNAL_HIDDEN */
bool pbl_bt_service_changed_client_notify_rx(struct pbl_bt_service_changed_client *client,
                                             uint16_t conn_handle, uint16_t attr_handle,
                                             struct os_mbuf *om);
/** @endcond */

/**
 * @brief Define a client.
 *
 * @param _name Name of the struct pbl_bt_service_changed_client.
 */
#define PBL_BT_SERVICE_CHANGED_CLIENT_DEFINE(_name)                                               \
  static struct pbl_bt_service_changed_client _name;                                              \
  static bool _name##_notify_rx(uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om) { \
    return pbl_bt_service_changed_client_notify_rx(&_name, conn_handle, attr_handle, om);         \
  }                                                                                               \
  PBL_BT_GATTC_NOTIFY_LISTENER_DEFINE(_name, _name##_notify_rx)

/**
 * @brief Look for the characteristic on a connection, and subscribe to it.
 *
 * A peer without it never changes its services, and nothing is reported.
 *
 * @param client Client.
 * @param conn_handle Connection.
 * @param cb Called when the services change.
 * @return 0 on success, -EALREADY if the client is started, another negative errno otherwise.
 */
int pbl_bt_service_changed_client_start(struct pbl_bt_service_changed_client *client,
                                        uint16_t conn_handle, pbl_bt_service_changed_cb_t cb);

/**
 * @brief Stop a client, e.g. when its connection is gone.
 *
 * @param client Client.
 */
void pbl_bt_service_changed_client_stop(struct pbl_bt_service_changed_client *client);

/** @} */
