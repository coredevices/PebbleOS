/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Increments the reconnect-pause counter. When the counter is above
//! 0, reconnection attempts will not occur.
void bt_driver_reconnect_pause(void);

//! Decrements the reconnect-pause counter. When the counter drops to
//! 0, reconnection attempts will be free to proceed.
void bt_driver_reconnect_resume(void);

//! Attempt to reconnect to the last connected remote device and restore
//! connections to the Bluetooth Classic profile (iSPP).
//! This is an asynchonous operation. A call to this function returns quickly.
//! If the last connected remote device and services are already connected, or
//! if the device is not an iOS device, this function does not do much.
//! @param ignore_paused If true, this call will attempt to reconnect,
//! even if the reconnection manager is paused. If false, the call will not
//! attempt to reconnect if the manager is paused.
void bt_driver_reconnect_try_now(bool ignore_paused);

//! Resets the reconnection manager's interval to the minimum interval, so
//! the rate of reconnection attempts is temporarily increased. This
//! should be called right after disconnecting or at any time that the remote
//! device might be suspected to be coming back in range.
void bt_driver_reconnect_reset_interval(void);

//! Notifies the BT Driver of the platform bitfield we received from the
//! 'session remote version endpoint'. (Some drivers cache this information to
//! determine BT connection behavior (such as the reconnection algorithm for
//! bluetooth classic)
void bt_driver_reconnect_notify_platform_bitfield(uint32_t platform_bitfield);
