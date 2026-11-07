/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/pp/session.h>
#include <pbl/util/uuid.h>

/**
 * @defgroup services_app_session App sessions
 * @ingroup services
 * @brief Pebble protocol session serving the running watch app.
 * @{
 */

/**
 * @brief Get the session serving the running watch app.
 *
 * For apps that use PebbleKit JS this is the system session.
 *
 * @return The session, or NULL if none can serve the app.
 */
struct pbl_pp_session *app_session_get_current(void);

/**
 * @brief Restrict a session to the one the running app may use.
 *
 * NULL selects the app's session; any other session is kept only if it is the app's session.
 *
 * @param[in,out] session Session to check; set to NULL if not permitted or none is open.
 */
void app_session_sanitize(struct pbl_pp_session **session);

/**
 * @brief Check whether the running app's phone counterpart announced a capability.
 *
 * Answers from a per-app cache while no session is open.
 *
 * @param capability Capability.
 * @return true if the capability was announced.
 */
bool app_session_has_capability(enum pbl_pp_capability capability);

/**
 * @brief Forget the cached capabilities of an app.
 *
 * @param app_uuid UUID of the app.
 */
void app_session_capabilities_evict(const Uuid *app_uuid);

/** @brief Compact the capability cache. Called once at boot. */
void app_session_capabilities_init(void);

/** @} */
