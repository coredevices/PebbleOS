/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "launcher_app_glance.h"

#include "applib/ui/kino/kino_reel.h"
#include "process_management/app_menu_data_source.h"
#include "services/normal/timeline/timeline_resources.h"

#define LAUNCHER_APP_GLANCE_GENERIC_ICON_SIZE_TYPE (TimelineResourceSizeTiny)

//! Create a generic launcher app glance for the provided app menu node.
//! @param node The node that the new generic glance should represent
//! @param fallback_icon A long-lived fallback icon to use if no other icons are available; will
//! not be destroyed when the generic glance is destroyed
//! @param fallback_icon_resource_id The resource ID of the fallback icon
LauncherAppGlance *launcher_app_glance_generic_create(const AppMenuNode *node,
                                                      const KinoReel *fallback_icon,
                                                      uint32_t fallback_icon_resource_id);
