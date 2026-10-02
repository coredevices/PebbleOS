/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>
#include <pbl/bluetooth/gatt_service_types.h>

/**
 * @defgroup bluetooth_gatt_discovery GATT service discovery
 * @ingroup bluetooth
 * @brief Discover the GATT database of a connected peer.
 *
 * Discovery is queued behind other GATT client procedures. Each primary service found within the
 * requested handle range is reported with pbl_bt_cb_gatt_client_discovery_handle_indication(),
 * followed by one pbl_bt_cb_gatt_client_discovery_complete(). Errors, including a failure to
 * start, are only reported through the latter. After a successful discovery the stack also
 * subscribes to the peer's Service Changed indications.
 *
 * @code{.c}
 * const struct pbl_bt_att_handle_range range = { .start = 0x0001, .end = 0xFFFF };
 *
 * pbl_bt_gatt_start_discovery_range(connection, &range);
 *
 * void pbl_bt_cb_gatt_client_discovery_handle_indication(GAPLEConnection *connection,
 *                                                        struct pbl_bt_gatt_service *service,
 *                                                        enum pbl_bt_errno error) {
 *   // Takes ownership of service, kernel_free() it when done.
 * }
 *
 * bool pbl_bt_cb_gatt_client_discovery_complete(GAPLEConnection *connection,
 *                                               enum pbl_bt_errno errno) {
 *   return true;
 * }
 * @endcode
 * @{
 */

/** @brief LE connection state kept by the firmware (see @c comm/ble/gap_le_connection.h). */
typedef struct GAPLEConnection GAPLEConnection;
struct pbl_bt_gatt_service;
/** @brief List node of a discovered service kept by the firmware. */
typedef struct GATTServiceNode GATTServiceNode;

/**
 * @brief Start discovering the services of a peer.
 *
 * @param connection The connection.
 * @param data Handle range to discover. Services not fully inside it are skipped.
 * @return PBL_BT_ERRNO_OK. Start failures are reported through
 *         pbl_bt_cb_gatt_client_discovery_complete().
 */
enum pbl_bt_errno pbl_bt_gatt_start_discovery_range(const GAPLEConnection *connection,
                                                    const struct pbl_bt_att_handle_range *data);
/**
 * @brief Stop an ongoing discovery.
 *
 * Blocks until the discovery has stopped. Must not be called from the NimBLE host task.
 *
 * @param connection The connection.
 * @retval PBL_BT_ERRNO_OK Stopped, or no discovery was in progress.
 * @retval PBL_BT_ERRNO_INVALID_STATE @p connection is not connected.
 */
enum pbl_bt_errno pbl_bt_gatt_stop_discovery(GAPLEConnection *connection);

/**
 * @brief Notify the stack that a discovery was abandoned.
 *
 * Called by the firmware when the connection or the stack goes away in the middle of a discovery,
 * so the stack can drop any state waiting for it to complete. Does nothing in the NimBLE backend.
 */
void pbl_bt_gatt_handle_discovery_abandoned(void);

/**
 * @brief Called when a discovery finished.
 *
 * @param connection The connection.
 * @param errno PBL_BT_ERRNO_OK on success, PBL_BT_ERRNO_SERVICE_DISCOVERY_TIMEOUT,
 *              PBL_BT_ERRNO_SERVICE_DISCOVERY_DISCONNECTED, or another error.
 * @return true if the discovery is finished, false if the firmware started a retry.
 */
extern bool pbl_bt_cb_gatt_client_discovery_complete(GAPLEConnection *connection,
                                                     enum pbl_bt_errno errno);
/**
 * @brief Called for each discovered service.
 *
 * @param connection The connection.
 * @param service_discovered The service, heap allocated. Ownership passes to the callee.
 * @param error PBL_BT_ERRNO_OK, or an error converting the service.
 */
extern void pbl_bt_cb_gatt_client_discovery_handle_indication(
    GAPLEConnection *connection, struct pbl_bt_gatt_service *service_discovered,
    enum pbl_bt_errno error);

/** @} */
