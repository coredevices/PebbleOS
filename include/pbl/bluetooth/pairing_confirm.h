/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @defgroup bluetooth_pairing_confirm Pairing confirmation
 * @ingroup bluetooth
 * @brief Ask the user to confirm a pairing request.
 *
 * @code{.c}
 * void pbl_bt_cb_pairing_confirm_handle_request(uint16_t conn_handle,
 *                                               const char *device_name,
 *                                               const char *confirmation_token) {
 *   // Show the token, then on the user's answer:
 *   pbl_bt_pairing_confirm(conn_handle, user_accepted);
 * }
 * @endcode
 * @{
 */

/**
 * @brief Answer a pairing request.
 *
 * @param conn_handle The connection that is pairing.
 * @param is_confirmed true if the user confirmed the pairing.
 */
void pbl_bt_pairing_confirm(uint16_t conn_handle, bool is_confirmed);

/**
 * @brief Called when a device requests pairing and the user must confirm it.
 *
 * @param conn_handle The connection that is pairing.
 * @param device_name Name of the device, or NULL if not available.
 * @param confirmation_token Numeric comparison value to display, or NULL if not available.
 * @note @p device_name and @p confirmation_token are only valid during the call.
 */
extern void pbl_bt_cb_pairing_confirm_handle_request(uint16_t conn_handle, const char *device_name,
                                                     const char *confirmation_token);

/**
 * @brief Called when a pairing process finished.
 *
 * @param conn_handle The connection that was pairing.
 * @param success true if the pairing succeeded.
 */
extern void pbl_bt_cb_pairing_confirm_handle_completed(uint16_t conn_handle, bool success);

/** @} */
