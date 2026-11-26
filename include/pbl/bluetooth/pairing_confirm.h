/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/kernel/section.h>

/**
 * @defgroup bluetooth_pairing_confirm Pairing confirmation
 * @ingroup bluetooth
 * @brief Ask the user to confirm a pairing request.
 *
 * @code{.c}
 * static void prv_request(uint16_t conn_handle, const char *device_name,
 *                         const char *confirmation_token) {
 *   // Show the token, then on the user's answer:
 *   pbl_bt_pairing_confirm(conn_handle, user_accepted);
 * }
 *
 * PBL_BT_PAIRING_LISTENER_DEFINE(my_ui, .request = prv_request);
 * @endcode
 * @{
 */

/** @brief What a pairing listener is told, on the NimBLE host task. Each is optional. */
struct pbl_bt_pairing_listener {
  /**
   * @brief A device requests pairing and the user must confirm it.
   *
   * @param conn_handle The connection that is pairing.
   * @param device_name Name of the device, or NULL if not available.
   * @param confirmation_token Numeric comparison value to display, or NULL if not available.
   * @note @p device_name and @p confirmation_token are only valid during the call.
   */
  void (*request)(uint16_t conn_handle, const char *device_name, const char *confirmation_token);
  /**
   * @brief A pairing process finished.
   *
   * @param conn_handle The connection that was pairing.
   * @param success true if the pairing succeeded.
   */
  void (*completed)(uint16_t conn_handle, bool success);
};

/**
 * @brief Define a listener of the pairings.
 *
 * @param _name Name of the listener.
 * @param ... Initializers of its struct pbl_bt_pairing_listener.
 */
#define PBL_BT_PAIRING_LISTENER_DEFINE(_name, ...)                            \
  static const struct pbl_bt_pairing_listener pbl_bt_pairing_listener_##_name \
  PBL_ITERABLE_SECTION(const struct pbl_bt_pairing_listener, bt_pairing, #_name) = {__VA_ARGS__}

/**
 * @brief Answer a pairing request.
 *
 * @param conn_handle The connection that is pairing.
 * @param is_confirmed true if the user confirmed the pairing.
 */
void pbl_bt_pairing_confirm(uint16_t conn_handle, bool is_confirmed);

/** @} */
