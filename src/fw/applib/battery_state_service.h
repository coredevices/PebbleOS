/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "services/common/battery/battery_monitor.h"

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup BatteryStateService
//!
//! \brief Determines when the battery state changes
//!
//! The BatteryStateService API lets you know when the battery state changes, that is,
//! its current charge level, whether it is plugged and charging. It uses the
//! BatteryChargeState structure to describe the current power state of Pebble.
//!
//! Refer to the <a href="https://github.com/pebble-examples/classio-battery-connection">
//! classio-battery-connection</a> example, which demonstrates using the battery state service
//! in a watchface.
//! @{

//! Callback type for battery state change events
//! @param charge the state of the battery \ref BatteryChargeState
typedef void (*BatteryStateHandler)(BatteryChargeState charge);


//! Subscribe to the battery state event service. Once subscribed, the handler gets called
//! on every battery state change
//! @param handler A callback to be executed on battery state change event
void battery_state_service_subscribe(BatteryStateHandler handler);

//! Unsubscribe from the battery state event service. Once unsubscribed, the previously registered
//! handler will no longer be called.
void battery_state_service_unsubscribe(void);

//! Peek at the last known battery state.
//! @return a \ref BatteryChargeState containing the last known data
BatteryChargeState battery_state_service_peek(void);

//!     @} // end addtogroup PEBBLE_BATTERY_STATE_CHANGE_EVENT
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation


