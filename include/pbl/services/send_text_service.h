/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

/**
 * @defgroup services_send_text_service Send text
 * @ingroup services
 * @brief Availability of the Send Text app.
 *
 * Caches whether the Send Text notification preferences hold a reply action. A user can have a
 * supported phone app but no supported carrier, in which case the app is hidden from the
 * launcher. Caching avoids flash reads in the launcher.
 * @{
 */

/** @brief Load the initial state and track preference changes. */
void send_text_service_init(void);

/**
 * @brief Check whether Send Text can be used.
 *
 * @return True if the phone app supports it and a reply action is configured.
 */
bool send_text_service_is_send_text_supported(void);

/** @} */
