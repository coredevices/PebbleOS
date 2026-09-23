/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/app_permissions/app_permissions_types.h"

#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Permissions
//! \brief Querying the permissions the user granted to your app
//!
//! Some capabilities, such as live microphone access, must be declared in your app's manifest
//! (`capabilities: ["microphone"]`) and granted by the user in the phone app. The grant can be
//! revoked at any time. Use this API to check the current state and to be told when it changes.
//! The system enforces the permission independently: an API that needs a permission fails when it
//! is not granted.
//!   @{

//! Handler called when the state of one of the app's declared permissions changes.
//! @param permission The permission that changed
//! @param state The new state
//! @param context The context passed to \ref app_permission_service_subscribe
typedef void (*AppPermissionChangedHandler)(AppPermission permission, AppPermissionState state,
                                            void *context);

//! Gets the state of a permission for the running app.
//! @return \ref AppPermissionStateNotDeclared if the app does not declare it in its manifest
AppPermissionState app_permission_get_state(AppPermission permission);

//! @return true if the permission is declared and granted
bool app_permission_is_granted(AppPermission permission);

//! Subscribes to permission changes. Only one handler can be registered at a time.
//! @param handler Handler called on the app task whenever a declared permission changes state
//! @param context User-provided context passed to the handler
void app_permission_service_subscribe(AppPermissionChangedHandler handler, void *context);

//! Unsubscribes from permission changes.
void app_permission_service_unsubscribe(void);

//!   @} // end addtogroup Permissions
//! @} // end addtogroup Foundation
