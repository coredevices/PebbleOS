/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/util/units.h>

/**
 * @defgroup services_bluetooth_ble_hrm Heart rate sharing
 * @ingroup services_bluetooth
 * @brief Shares heart rate over the BLE Heart Rate Service, with per-device user permission.
 *
 * When a device subscribes, the user is asked for permission. While sharing, the heart rate
 * monitor runs and a reminder popup is shown periodically. Available with
 * @c CONFIG_SERVICE_BLE_HRM.
 * @{
 */

/** @brief Interval between sharing reminder popups, in minutes. */
#define BLE_HRM_REMINDER_POPUP_DELAY_MINS (2 * PBL_MIN_PER_HOUR)

/** @cond INTERNAL_HIDDEN */
typedef struct GAPLEConnection GAPLEConnection;
/** @endcond */

/** @brief Opaque pending request for permission to share. */
typedef struct BLEHRMSharingRequest BLEHRMSharingRequest;

/**
 * @brief Apply the user's answer to a sharing request.
 *
 * Called by the sharing popup. Frees @p sharing_request.
 *
 * @param is_granted True if the user allowed sharing.
 * @param sharing_request The request being answered.
 */
void ble_hrm_handle_sharing_request_response(bool is_granted,
                                             BLEHRMSharingRequest *sharing_request);

/**
 * @brief Check whether heart rate sharing is possible.
 *
 * @return True if the stack and hardware support it and heart rate monitoring is enabled.
 */
bool ble_hrm_is_supported_and_enabled(void);

/**
 * @brief Check whether heart rate is shared with a connection.
 *
 * The caller must hold bt_lock().
 *
 * @param connection LE connection, may be NULL.
 * @return True if subscribed and permitted.
 */
bool ble_hrm_is_sharing_to_connection(const GAPLEConnection *connection);

/**
 * @brief Check whether heart rate is shared with any device.
 *
 * @return True if sharing.
 */
bool ble_hrm_is_sharing(void);

/**
 * @brief Revoke sharing permission for a connection.
 *
 * Disconnects the link, since the server cannot remove a client's subscription.
 *
 * @param connection LE connection.
 */
void ble_hrm_revoke_sharing_permission_for_connection(GAPLEConnection *connection);

/**
 * @brief Revoke sharing permission for all connections, disconnecting them.
 */
void ble_hrm_revoke_all(void);

/**
 * @brief Handle a change of the heart rate monitoring preference.
 *
 * Disabling it ends all sharing.
 *
 * @param is_enabled New preference value.
 */
void ble_hrm_handle_activity_prefs_heart_rate_is_enabled(bool is_enabled);

/**
 * @brief Handle an LE disconnection.
 *
 * If the device was being shared with, briefly advertises the Heart Rate Service to help it
 * reconnect. Permissions are kept. The caller must hold bt_lock().
 *
 * @param connection Disconnected connection.
 */
void ble_hrm_handle_disconnection(GAPLEConnection *connection);

/**
 * @brief Initialize heart rate sharing when the stack starts.
 */
void ble_hrm_init(void);

/**
 * @brief Stop sharing and forget all permissions when the stack stops.
 */
void ble_hrm_deinit(void);

/** @} */
