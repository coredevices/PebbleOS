/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup BacklightService
//!
//! \brief Notifies your app when the backlight turns on or off.
//!
//! The BacklightService lets your app react to the backlight turning on.
//! The handler is invoked whenever the backlight transitions
//! between off and on (any non-off state — fully on, on with timeout, or
//! fading out is treated as "on"). This means you get a single edge per
//! wake, not one event per fade step.
//!     @{

//! Callback type for backlight on/off events.
//! @param on true when the backlight has just turned on, false when it has
//!   just turned fully off.
typedef void (*BacklightHandler)(bool on);

//! Subscribe to the backlight event service. Once subscribed, the handler is
//! called every time the backlight transitions between off and on.
//! @param handler A callback to be executed on backlight on/off events.
void backlight_service_subscribe(BacklightHandler handler);

//! Unsubscribe from the backlight event service. Once unsubscribed, the
//! previously registered handler will no longer be called.
//!
//! To read the current backlight state at any time, use \ref light_is_on().
void backlight_service_unsubscribe(void);

//!     @} // end addtogroup BacklightService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
