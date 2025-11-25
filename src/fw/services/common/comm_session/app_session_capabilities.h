/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/comm_session/session.h"
#include "util/uuid.h"

//! @param capability The capability to check for.
//! @returns True if the session for the current application supports the capability of interest.
//! If the session is currently not connected, it will use cached data. If no cache exists
//! and the session is not connected, false will be returned.
bool comm_session_current_app_session_cache_has_capability(CommSessionCapability capability);

//! Removes the cached app session capabilities for app with specified uuid.
void comm_session_app_session_capabilities_evict(const Uuid *app_uuid);

void comm_session_app_session_capabilities_init(void);
