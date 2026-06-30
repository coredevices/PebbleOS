/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <bluetooth/bluetooth_types.h>

#include <stdint.h>

//! @file Kernel <-> BT driver API for the "reversed PPoG" GATT service: the
//! watch hosts the service and the phone is the GATT client. Outbound bytes
//! leave via notifications on the data-notify characteristic; inbound bytes
//! arrive as write-without-response writes on the data-write characteristic.

//! All bt_driver_cb_ppog_reversed_* callbacks are invoked from NimBLE host-task
//! callbacks (no bt_lock held). The kernel-side implementations defer the
//! actual work to KernelBG to avoid stacking on top of NimBLE's already-deep
//! frames — running the PPoG state machine + bt_lock acquisition + further
//! NimBLE calls from this task can overflow the 5000-byte host stack.

//! Driver -> kernel: the phone enabled notifications on the data-notify
//! characteristic.
extern void bt_driver_cb_ppog_reversed_subscribed(const BTDeviceInternal *device,
                                                  uint16_t conn_handle);

//! Driver -> kernel: the phone disabled notifications or disconnected.
extern void bt_driver_cb_ppog_reversed_unsubscribed(uint16_t conn_handle);

//! Driver -> kernel: the phone wrote a PPoG packet to the data-write
//! characteristic. Ownership of @p buf transfers to the kernel, which frees it
//! after the deferred handler runs. @p buf must have been allocated with
//! kernel_malloc / kernel_zalloc.
extern void bt_driver_cb_ppog_reversed_data_written(uint16_t conn_handle,
                                                    uint8_t *buf, uint16_t len);

//! Driver -> kernel: a previously-queued notification finished transmitting,
//! freeing host buffers. The kernel drains any backlog accumulated while we
//! were getting BLE_HS_ENOMEM.
extern void bt_driver_cb_ppog_reversed_notify_tx_complete(void);

//! Kernel -> driver: send a PPoG packet to the phone as a notification on the
//! reversed PPoG data-notify characteristic.
//! @return BTErrnoOK on success, BTErrnoNotEnoughResources if NimBLE is out of
//! mbufs (caller should wait for bt_driver_cb_ppog_reversed_notify_tx_complete
//! and retry), BTErrnoInvalidState if no subscription is active.
BTErrno bt_driver_ppog_reversed_notify(uint16_t conn_handle,
                                       const uint8_t *buf, uint16_t len);
