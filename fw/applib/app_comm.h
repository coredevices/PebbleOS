/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

//! @addtogroup Foundation
//! @{
//!   @addtogroup AppComm App Communication
//!   \brief API for interacting with the Pebble communication subsystem.
//!
//! @note To send messages to a remote device, see the \ref AppMessage or
//! \ref AppSync modules.
//!   @{

//! Intervals during which the Bluetooth module may enter a low power mode.
//! The sniff interval defines the period during which the Bluetooth module may
//! not exchange (ACL) packets. The longer the sniff interval, the more time the
//! Bluetooth module may spend in a low power mode.
//! It may be necessary to reduce the sniff interval if an app requires reduced
//! latency when sending messages.
//! @note These settings have a dramatic effect on the Pebble's energy
//! consumption. Use the normal sniff interval whenever possible.
//! Note, however, that switching between modes increases power consumption
//! during the process. Frequent switching between modes is thus
//! discouraged. Ensure you do not drop to normal frequently. The Bluetooth module
//! is a major consumer of the Pebble's energy.

typedef enum {
  //! Set the sniff interval to normal (power-saving) mode
  SNIFF_INTERVAL_NORMAL = 0,
  //! Reduce the sniff interval to increase the responsiveness of the radio at
  //! the expense of increasing Bluetooth energy consumption by a multiple of 2-5
  //! (very significant)
  SNIFF_INTERVAL_REDUCED = 1,
} SniffInterval;

//! Set the Bluetooth module's sniff interval.
//! The sniff interval will be restored to normal by the OS after the app's
//! de-init handler is called. Set the sniff interval to normal whenever
//! possible.
void app_comm_set_sniff_interval(const SniffInterval interval);

//! Get the Bluetooth module's sniff interval
//! @return The SniffInterval value corresponding to the current interval
SniffInterval app_comm_get_sniff_interval(void);

//!   @} // end addtogroup AppComm
//! @} // end addtogroup Foundation
