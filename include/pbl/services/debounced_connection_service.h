/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <kernel/events.h>

/**
 * @defgroup services_debounced_connection_service Debounced connection service
 * @ingroup services
 * @brief Hides short phone disconnections from apps.
 *
 * Tracks the connection to the mobile app (system session) and to third-party apps using
 * PebbleKit. Connections are reported right away with a
 * @c PBL_BT_PEBBLE_CONNECTION_DEBOUNCED_EVENT, but a disconnection is only reported, with the
 * disconnection vibe, if the connection is not restored within 25 seconds. This makes the
 * connection appear more reliable.
 * @{
 */

/** @brief Initialize the service from the current connection state. */
void debounced_connection_service_init(void);

/**
 * @brief Handle a comm session open or close event.
 *
 * @param e Comm session event.
 */
void debounced_connection_service_handle_event(PebbleCommSessionEvent *e);

/** @} */
