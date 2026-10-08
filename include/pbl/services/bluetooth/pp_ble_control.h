/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/services/comm_session/session.h>

/**
 * @defgroup services_bluetooth_pp_ble_control BLE control endpoint
 * @ingroup services_bluetooth
 * @brief Pebble Protocol endpoint letting the phone make the watch discoverable.
 * @{
 */

/**
 * @brief Handle a BLE control endpoint message, on KernelBG.
 *
 * Only the "set discoverable and pairable" command is supported; it makes the watch pairable
 * for the requested duration.
 *
 * @param session Session the message arrived on.
 * @param data Message payload.
 * @param length Length of @p data in bytes.
 */
void pp_ble_control_protocol_msg_callback(CommSession *session, const uint8_t *data,
                                          unsigned int length);

/** @} */
