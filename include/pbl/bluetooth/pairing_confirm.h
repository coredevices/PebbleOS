/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup bluetooth_pairing_confirm Pairing confirmation
 * @ingroup bluetooth
 * @brief Ask the user to confirm a pairing request.
 *
 * @code{.c}
 * void pbl_bt_cb_pairing_confirm_handle_request(const struct pbl_bt_pairing_confirm_ctx *ctx,
 *                                               const char *device_name,
 *                                               const char *confirmation_token) {
 *   // Show the token, then on the user's answer:
 *   pbl_bt_pairing_confirm(ctx, user_accepted);
 * }
 * @endcode
 * @{
 */

/** @brief Opaque, backend specific pairing process context. */
struct pbl_bt_pairing_confirm_ctx;

/**
 * @brief Answer a pairing request.
 *
 * @param ctx The pairing process, as passed to pbl_bt_cb_pairing_confirm_handle_request().
 * @param is_confirmed true if the user confirmed the pairing.
 */
void pbl_bt_pairing_confirm(const struct pbl_bt_pairing_confirm_ctx *ctx, bool is_confirmed);

/**
 * @brief Called when a device requests pairing and the user must confirm it.
 *
 * @param ctx Identifies the pairing process. Compare it to tell processes apart, but never
 *            dereference it: it may be released at any time, for example when the pairing
 *            times out. The NimBLE backend uses the connection handle.
 * @param device_name Name of the device, or NULL if not available.
 * @param confirmation_token Numeric comparison value to display, or NULL if not available.
 * @note @p device_name and @p confirmation_token are only valid during the call.
 */
extern void pbl_bt_cb_pairing_confirm_handle_request(const struct pbl_bt_pairing_confirm_ctx *ctx,
                                                     const char *device_name,
                                                     const char *confirmation_token);

/**
 * @brief Called when a pairing process finished.
 *
 * @param ctx The pairing process, see pbl_bt_cb_pairing_confirm_handle_request().
 * @param success true if the pairing succeeded.
 */
extern void pbl_bt_cb_pairing_confirm_handle_completed(const struct pbl_bt_pairing_confirm_ctx *ctx,
                                                       bool success);

/** @} */
