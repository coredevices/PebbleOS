/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_phone_call Phone calls
 * @ingroup services
 * @brief Incoming call handling.
 *
 * Calls are reported over ANCS on iOS and over the Pebble Protocol phone endpoint otherwise
 * (see @ref services_phone_pp). The service consumes @c PEBBLE_PHONE_EVENT events, drives the
 * phone call UI, and routes answer and decline actions to the right transport.
 * @{
 */

/** @brief Initialize the service and subscribe to the events it handles. */
void phone_call_service_init(void);

/**
 * @brief Answer the current incoming call.
 *
 * On iOS the call is considered over for the watch once answered, as no ongoing call UI is
 * shown.
 */
void phone_call_answer(void);

/** @brief Decline or hang up the current call. */
void phone_call_decline(void);

/** @} */
