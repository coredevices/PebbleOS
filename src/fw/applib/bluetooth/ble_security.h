/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

//------------------------------------------------------------------------------
// Out-Of-Band additions


//! "Out-of-Band" (OOB) is one of the mechanisms to exchange a shared secret
//! during a pairing procedure between two devices. "PIN" and "Just Works" are
//! the two other exchange mechanisms that the Bluetooth 4.0 Specification
//! defines, but both are susceptible to eavesdropping of the exchanged keys.
//! OOB provides better protection against this, by offering a way to exchange
//! the shared secret via a communications channel other than Bluetooth itself
//! (hence the name "Out-of-Band"). Of course, this is only more secure if the
//! channel through which the OOB data is exchanged itself is harder to
//! eavesdrop.
//!
//! The exchanged OOB data is used as Temporary-Key (TK) to encrypt the
//! connection during the one-time pairing information exchange. Part of this
//! information exchange are Long-Term-Key(s) (LTK) that will be used upon
//! successive reconnections. For more details, see Bluetooth 4.0 Specification,
//! Volume 3, Part H, 2.3.5, "Pairing Algorithms".
//!
//! The OOB APIs enable the application to provide the system with OOB data.
//! The application will need to indicate to the system for what devices it
//! is capable of providing OOB data. Later, when a pairing procedure takes
//! place with an OOB-enabled device, the system will ask the application to
//! provide that OOB data.
//!
//! It is up to the application and the manufacturer of the device how the OOB
//! data is exchanged between the application and the remote device. Examples of
//! how this can be done:
//! - The application could generate the OOB data and show a QR code containing
//! the data on the screen of the Pebble that is then read by the device.
//! - If the device is connected to the Internet, the OOB data could be
//! provisioned to Pebble via a web service. The application would use the
//! JavaScript APIs to fetch the data from the web service and transfer the
//! data to the application on the watch using the AppMessage APIs.


//! Pointer to a function that can provide Out-Of-Band keys.
//! @see ble_security_set_oob_handler() and ble_security_enable_oob()
//! @param device The device for which the OOB key needs to be provided
//! @param oob_key_buffer_out The buffer into which the OOB key should be
//! written.
//! @param oob_key_buffer_size The size of the buffer in bytes. Currently only
//! keys of 128-bit (16 byte) size are supported.
//! @return true if the OOB key was written or false if no OOB data could be
//! provided for the device.
typedef bool (*BLESecurityOOBHandler)(BTDevice device,
                                      uint8_t *oob_key_buffer_out,
                                      size_t oob_key_buffer_size);

//! Registers a permanent callback function that is responsible for providing
//! Out-Of-Band (OOB) keys. The callback is guaranteed to get called only for
//! devices for which the application has enabled OOB using
//! ble_security_enable_oob(). The callback will get called by the system during
//! a pairing procedure, but only if the remote device indicated to have OOB
//! data as well.
//! @param oob_handler Pointer to the function that will provide OOB key data.
//! @return BTErrnoOK if the call was successful, or TODO...
BTErrno ble_security_set_oob_handler(BLESecurityOOBHandler oob_handler);

//! Enable or disable Out-Of-Band pairing for the device.
//! This function is a way to indicate to the system that the application has
//! Out-Of-Band data that can be used when pairing with a particular device.
//! @note The application is encouraged to configure OOB as soon as possible,
//! *before* connecting to any devices. If the application supports OOB, but
//! enables is *after* connecting, there is a chance that the remote requests to
//! start pairing before your application has had the chance to enable OOB.
//! @note After terminating the application, the system will automatically
//! disable OOB for any devices it had enabled OOB for. Upon re-launching the
//! application, it will need to re-enable OOB if required.
//! @param device The device for which to enable or disable OOB
//! @param enable Pass in true to enable OOB for the device, or false to disable
//! @return BTErrnoOK if the call was successful, or TODO...
BTErrno ble_security_enable_oob(BTDevice device, bool enable);
