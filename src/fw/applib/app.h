/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "process_management/app_install_types.h"

//! @file app.h
//!
//! @addtogroup Foundation
//! @{
//!   @addtogroup App
//!   @{

//! @internal
//! Requests the app to re-render by scheduling its top window to be rendered.
void app_request_render(void);

//! @internal
//! Event loop that is shared between Rocky.js and C apps. This is called by both app_event_loop()
//! as well as rocky_event_loop_with_...().
void app_event_loop_common(void);

//! The event loop for C apps, to be used in app's main().
//! Will block until the app is ready to exit.
void app_event_loop(void);

//! @internal
//! Get the AppInstallId for the current app or worker
AppInstallId app_get_app_id(void);

//!   @} // end addtogroup App
//! @} // end addtogroup Foundation
