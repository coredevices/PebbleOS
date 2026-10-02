/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/comm_session/session.h"
#include "pbl/util/uuid.h"

/**
 * @defgroup services_comm_session_app_session_capabilities App session capability cache
 * @ingroup services_comm_session
 * @brief Per-app cache of app session capabilities, persisted in a settings file.
 * @{
 */

/**
 * @brief Check whether the current app's session supports a capability.
 *
 * When the app session is connected its capabilities are used and the cache is refreshed;
 * otherwise the cached value is used.
 *
 * @param capability Capability to look for.
 * @return True if supported; false if not, or if disconnected with nothing cached.
 */
bool comm_session_current_app_session_cache_has_capability(CommSessionCapability capability);

/**
 * @brief Remove the cached capabilities of an app.
 *
 * @param app_uuid UUID of the app.
 */
void comm_session_app_session_capabilities_evict(const Uuid *app_uuid);

/**
 * @brief Initialize the cache, compacting its settings file.
 */
void comm_session_app_session_capabilities_init(void);

/** @} */
