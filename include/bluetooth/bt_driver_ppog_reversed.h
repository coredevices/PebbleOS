/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

#include <stdint.h>

//! @file Kernel <-> BT driver API for the reversed PPoG GATT service: the
//! watch hosts the service and the phone is the GATT client.
//! The bt_driver_cb_ppog_reversed_* callbacks are invoked from the BT driver
//! task without bt_lock held.

//! Driver -> kernel: the phone enabled notifications on the data-notify
//! characteristic.
extern void bt_driver_cb_ppog_reversed_subscribed(const BTDeviceInternal *device,
                                                  uint16_t conn_handle);

//! Driver -> kernel: the phone disabled notifications or disconnected.
extern void bt_driver_cb_ppog_reversed_unsubscribed(uint16_t conn_handle);

//! Driver -> kernel: the phone wrote a PPoG packet to the data-write
//! characteristic. Ownership of @p buf (kernel heap) transfers to the kernel.
extern void bt_driver_cb_ppog_reversed_data_written(uint16_t conn_handle,
                                                    uint8_t *buf, uint16_t len);

//! Kernel -> driver: send a PPoG packet to the phone as a notification.
//! @return BTErrnoOK on success, BTErrnoNotEnoughResources if out of buffers
//! (transient; the caller must retry after a short delay — the stack has no
//! buffers-freed event), or BTErrnoInvalidState if no subscription is active.
BTErrno bt_driver_ppog_reversed_notify(uint16_t conn_handle,
                                       const uint8_t *buf, uint16_t len);
