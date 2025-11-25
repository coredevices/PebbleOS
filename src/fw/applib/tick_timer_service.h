/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <time.h>


//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup TickTimerService
//! \brief Handling time components
//!
//! The TickTimerService allows your app to be called every time one Time component has changed.
//! This is extremely important for watchfaces. Your app can choose on which time component
//! change a tick should occur. Time components are defined by a \ref TimeUnits enum bitmask.
//! @{

//! Time unit flags that can be used to create a bitmask for use in \ref tick_timer_service_subscribe().
//! This will also be passed to \ref TickHandler.
typedef enum {
  //! Flag to represent the "seconds" time unit
  SECOND_UNIT = 1 << 0,
  //! Flag to represent the "minutes" time unit
  MINUTE_UNIT = 1 << 1,
  //! Flag to represent the "hours" time unit
  HOUR_UNIT = 1 << 2,
  //! Flag to represent the "days" time unit
  DAY_UNIT = 1 << 3,
  //! Flag to represent the "months" time unit
  MONTH_UNIT = 1 << 4,
  //! Flag to represent the "years" time unit
  YEAR_UNIT = 1 << 5
} TimeUnits;

//! Callback type for tick timer events
//! @param tick_time the time at which the tick event was triggered
//! @param units_changed which unit change triggered this tick event
typedef void (*TickHandler)(struct tm *tick_time, TimeUnits units_changed);

//! Subscribe to the tick timer event service. Once subscribed, the handler gets called
//! on every requested unit change.
//! Calling this function multiple times will override the units and handler (i.e., only 
//! the last tick_units and handler passed will be used).
//! @param handler The callback to be executed on tick events
//! @param tick_units a bitmask of all the units that have changed
void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler);

//! Unsubscribe from the tick timer event service. Once unsubscribed, the previously registered
//! handler will no longer be called.
void tick_timer_service_unsubscribe(void);

//!     @} // end addtogroup TickTimerService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation

