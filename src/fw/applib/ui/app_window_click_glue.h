/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/app_manager.h"
#include "applib/ui/click_internal.h"

////////////////////////////////////////////////
// App + Click Recognizer + Window = Glue code

//! Calls the provider function of the window with the ClickConfig structs of the "app global" click recognizers.
//! The window is set as context to of each of the ClickConfig's .context fields for convenience.
//! In case window has a click_config_context set, it will use that as context instead of the window itself.
//! @see AppContext.click_recognizer[]
struct Window;

void app_click_config_setup_with_window(ClickManager* click_manager, struct Window *window);

