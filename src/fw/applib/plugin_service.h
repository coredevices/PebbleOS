/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include "util/uuid.h"

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup PlugInService
//!
//! \brief Using the PlugInSErvice
//!
//! The PlugInService enables 3rd party apps to publish and subscribe to events for a custom service. For example,
//! a background worker could publish events for a custom service and any foreground app that wants to can subscribe
//! to those events.
//!
//! Plug-in services are identified by UUID. The client of a service will get a pointer to an event structure
//! param block whose content is unique to each service.
//!
//!     @{


//! Generic structure of a plug-in event that will be received by an app
typedef struct {
  uint16_t data0;
  uint16_t data1;
  uint16_t data2;
} PluginEventData;

//! Callback type for plug-in service events
//! @param type the event type
//! @param data pointer to event data. The client must know the structure of the data provided by the plug-in service.
typedef void (*PluginServiceHandler)(uint8_t type, PluginEventData *data);

//! Subscribe to a specific plugin service. Once subscribed, the handler
//! gets called on every event emitted by that service.
//! @param uuid The UUID of the plug-in service
//! @param handler A callback to be executed when the event is received
//! @return true on success
bool plugin_service_subscribe(Uuid *uuid, PluginServiceHandler handler);

//! Unsubscribe from a plugin service. Once unsubscribed,
//! the previously registered handler will no longer be called.
//! @return true on success
bool plugin_service_unsubscribe(Uuid *uuid);


//! Send an event for a plug-in service
//! @param type the event type
//! @param data the event data structure
void plugin_service_send_event(Uuid *uuid, uint8_t type, PluginEventData *data);


//!     @} // end addtogroup PlugInService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation

//! @internal
//! Register the accelerometer service with the event service system
void plugin_service_init(void);

