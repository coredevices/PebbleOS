/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup EventService
//!   @{
//!     @addtogroup ConnectionService
//! \brief Determine what the Pebble watch is connected to
//!
//! The ConnectionService allows your app to learn about the apps the Pebble
//! watch is connected to. You can ask the system for this information at a
//! given time or you can register to receive events every time connection or
//! disconnection events occur.
//!
//! It allows you to determine whether the watch is connected to the Pebble
//! mobile app by subscribing to the pebble_app_connection_handler or by calling
//! the connection_service_peek_pebble_app_connection function.  Note that when
//! the Pebble app is connected, you can assume PebbleKit JS apps will also be
//! running correctly.
//!
//! The service also allows you to determine if the Pebble watch can establish
//! a connection to a PebbleKit companion app by subscribing to the
//! pebblekit_connection_handler or by calling the
//! connection_service_peek_pebblekit_connection function.  Today, due to
//! architectural differences between iOS and Android, this will return true
//! for Android anytime a connection with the Pebble mobile app is established
//! (since PebbleKit messages are routed through the Android app). For iOS,
//! this will return true when any PebbleKit companion app has established a
//! connection with the Pebble watch (since companion app messages are routed
//! directly to the watch)
//!
//!     @{
//! Callback type for connection events
//! @param connected true on connection, false on disconnection
typedef void (*ConnectionHandler)(bool connected);

//! Query the bluetooth connection service for the current Pebble app connection status
//! @return true if the Pebble app is connected, false otherwise
bool connection_service_peek_pebble_app_connection(void);

//! Query the bluetooth connection service for the current PebbleKit connection status
//! @return true if a PebbleKit companion app is connected, false otherwise
bool connection_service_peek_pebblekit_connection(void);

typedef struct {
  //! callback to be executed when the connection state between the watch and
  //! the phone app has changed. Note, if the phone App is connected, PebbleKit JS apps
  //! will also be working correctly
  ConnectionHandler pebble_app_connection_handler;
  //! ID for callback to be executed on PebbleKit connection event
  ConnectionHandler pebblekit_connection_handler;
} ConnectionHandlers;

//! Subscribe to the connection event service. Once subscribed, the appropriate
//! handler gets called based on the type of connection event and user provided
//! handlers
//! @param ConnectionHandlers A struct populated with the handlers to
//! be called when the specified connection event occurs. If a given handler is
//! NULL, no function will be called.
void connection_service_subscribe(ConnectionHandlers conn_handlers);

//! Unsubscribe from the bluetooth event service. Once unsubscribed, the previously registered
//! handler will no longer be called.
void connection_service_unsubscribe(void);

//! @deprecated Backwards compatibility typedef for ConnectionHandler. New code
//! should use ConnectionHandler directly.  This will be removed in a future
//! version of the Pebble SDK.
typedef ConnectionHandler BluetoothConnectionHandler;

//! @deprecated Backward compatibility function for
//! connection_service_subscribe.  New code should use
//! connection_service_subscribe directly. This will be removed in a future
//! version of the Pebble SDK
void bluetooth_connection_service_subscribe(ConnectionHandler handler);

//! @deprecated Backward compatibility function for
//! connection_service_unsubscribe.  New code should use
//! connection_service_unsubscribe directly. This will be removed in a future
//! version of the Pebble SDK
void bluetooth_connection_service_unsubscribe(void);

//! @deprecated Backward compatibility function for
//! connection_service_peek_pebble_app_connection.  New code should use
//! connection_service_peek_pebble_app_connection directly. This will be
//! removed in a future version of the Pebble SDK
bool bluetooth_connection_service_peek(void);

//!     @} // end addtogroup ConnectionService
//!   @} // end addtogroup EventService
//! @} // end addtogroup Foundation
