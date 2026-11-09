/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup DarkModeService
//!
//! \brief Determines whether the user has dark mode on
//!
//! Dark mode is a system setting: off, on, or on a schedule. The system UI follows it, while apps
//! keep the colors they choose. An app that wants to match it checks the current state with
//! \ref dark_mode_service_peek and subscribes to learn when it changes, for example when a
//! schedule starts or ends while the app is open.
//! @{

//! Callback type for dark mode change events
//! @param is_dark Whether dark mode is now on
typedef void (*DarkModeHandler)(bool is_dark);

//! Subscribe to the dark mode event service. Once subscribed, the handler gets called every time
//! dark mode turns on or off.
//! @param handler A callback to be executed when dark mode turns on or off
void dark_mode_service_subscribe(DarkModeHandler handler);

//! Unsubscribe from the dark mode event service. Once unsubscribed, the previously registered
//! handler will no longer be called.
void dark_mode_service_unsubscribe(void);

//! Peek at whether dark mode is on. In Schedule mode, this is whether the schedule is in its dark
//! period right now.
//! @return true if dark mode is on
bool dark_mode_service_peek(void);

//!     @} // end addtogroup DarkModeService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
