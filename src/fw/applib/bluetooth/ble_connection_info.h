/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

//! @addtogroup Foundation
//! @{
//!   @addtogroup Bluetooth
//!   @{
//!     @addtogroup BLEConnectionInfo
//! \brief Get information about the current Bluetooth connection
//!
//! The BLE Connection Info API allows your app to retrieve information
//! about the current Bluetooth Low Energy connection to the connected device
//! (typically the phone running the Pebble mobile app).
//!
//!     @{

//! Get the RSSI (Received Signal Strength Indicator) of the current
//! Bluetooth connection to the gateway device (phone).
//! 
//! The RSSI value is a negative number expressed in dBm (decibels relative to
//! one milliwatt). Typical values range from -30 dBm (very strong signal, very close)
//! to -100 dBm (very weak signal, far away or obstructed).
//! 
//! Generally:
//! - RSSI >= -50 dBm: Excellent signal strength
//! - RSSI between -50 and -70 dBm: Good signal strength
//! - RSSI between -70 and -85 dBm: Fair signal strength
//! - RSSI < -85 dBm: Poor signal strength
//! 
//! @param rssi_out Pointer to store the RSSI value. Will only be updated if
//! the function returns true.
//! @return true if RSSI was successfully retrieved, false if there is no
//! active Bluetooth connection or the RSSI could not be determined.
//! 
//! @note This function returns the RSSI for the gateway connection (the phone),
//! not for BLE peripheral connections established by the watch.
bool bluetooth_connection_get_rssi(int8_t *rssi_out);

//!     @}
//!   @}
//! @}


