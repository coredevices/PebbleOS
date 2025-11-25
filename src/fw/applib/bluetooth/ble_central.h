/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

//! Callback that is called for each connection and disconnection event.
//! @param device The device that got (dis)connected
//! @param connection_status BTErrnoConnected if connected, otherwise the
//! reason for the disconnection: BTErrnoConnectionTimeout,
//! BTErrnoRemotelyTerminated, BTErrnoLocallyTerminatedBySystem or
//! BTErrnoLocallyTerminatedByApp.
//! @note See additional notes with ble_central_set_connection_handler()
typedef void (*BLEConnectionHandler)(BTDevice device,
                                     BTErrno connection_status);

//! Registers the connection event handler of the application.
//! This event handler will be called when connections and disconnection occur,
//! for devices for which ble_central_connect() has been called by the
//! application.
//! Only for successful connections and complete disconnections will the event
//! handler be called. Transient issues that might happen during connection
//! establishment will be not be reported to the application. Instead, the
//! system will attempt to initiate a connection to the device again.
//! If this is called again, the previous handler will be unregistered.
//! @param handler The connection event handler of the application
//! @return Always returns BTErrnoOK.
BTErrno ble_central_set_connection_handler(BLEConnectionHandler handler);

//! Attempts to initiate a connection from the application to another device.
//! In case there is no Bluetooth connection to the device yet, this function
//! configures the Bluetooth subsystem to scan for the specified
//! device and instructs it to connect to it as soon as a connectable
//! advertisement has been received. The application will need to register a
//! connection event handler prior to calling ble_central_connect(), using
//! ble_central_set_connection_handler().
//! Outstanding (auto)connection attempts can be cancelled using
//! ble_central_cancel_connect().
//! @note Connections are virtualized. This means that your application needs to
//! call the ble_central_connect, even though the device might already have an
//! actual Bluetooth connection already. This can be the case when connecting
//! to the user's phone: it is likely the system has created a Bluetooth
//! connection already, still the application has to connect internally in order
//! to use the connection.
//! @param device The device to connect to
//! @param auto_reconnect Pass in true to automatically attempt to reconnect
//! again if the connection is lost. The BLEConnectionHandler will be called for
//! each time the device is (re)connected.
//! Pass in false, if the system should connect once only.
//! The BLEConnectionHandler will be called up to one time only.
//! @param is_pairing_required If the application requires that the Bluetooth
//! traffic is encrypted, is_pairing_required can be set to true to let the
//! system transparently set up pairing, or reestablish encryption, if the
//! device is already paired. Only after this security procedure is finished,
//! the BLEConnectionHandler will be called. Note that not all devices support
//! pairing and one of the BTErrnoPairing... errors can result from a failed
//! pairing process. If the application does not require pairing, set to false.
//! @note It is possible that encryption is still enabled, even if the
//! application did not require this.
//! @return BTErrnoOK if the intent to connect was processed successfully, or
//! ... TODO
BTErrno ble_central_connect(BTDevice device,
                            bool auto_reconnect,
                            bool is_pairing_required);

//! Attempts to cancel the connection, as initiated by ble_central_connect().
//! The underlying Bluetooth connection might not be disconnected if the
//! connection is still in use by the system. However, as far as the application
//! is concerned, the device is disconnected and the connection handler will
//! be called with BTErrnoLocallyTerminatedByApp.
//! @return BTErrnoOK if the cancelling was successful, or ... TODO
BTErrno ble_central_cancel_connect(BTDevice device);
