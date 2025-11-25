/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/app_smartstrap.h"
#include "kernel/events.h"
#include "services/normal/accessory/smartstrap_profiles.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * This module creates kernel-space structs to represent SmartstrapAttributes for the app. Because
 * it is dealing with app buffers, the kernel-space structs are kept within smartstrap_attribute.c
 * and they are referenced via the APIs using the user-space SmartstrapAttribute pointer. This
 * SmartstrapAttribute pointer is actually just the user-space buffer pointer, but this fact is
 * hidden from apps (and not particularly useful for them to know).
 */

/*
 * FSM state transitions:
 * +-------------------+-------------------+----------+---------------------------+
 * | From State        | To State          | Task     | Event                     |
 * +-------------------+-------------------+----------+---------------------------+
 * | Idle              | RequestPending    | App      | *_read()                  |
 * | Idle              | WritePending      | App      | *_begin_write()           |
 * | WritePending      | RequestPending    | App      | *_end_write()             |
 * | WritePending      | Idle              | App      | *_end_write() failed      |
 * | RequestPending    | Idle              | KernelBG | Failed to send request    |
 * | RequestPending    | RequestInProgress | KernelBG | Request sent successfully |
 * | RequestInProgress | Idle              | KernelBG | Got response to request   |
 * +-------------------+-------------------+----------+---------------------------+
 * Notes:
 * - Only the App task can move out of the Idle and WritePending states
 * - Only KernelBG can move out of the RequestPending and RequestInProgress state
 * - The lower-level smartstrap sending APIs are called only from KernelBG so we don't block the App
 */
typedef enum {
  SmartstrapAttributeStateIdle = 0,
  SmartstrapAttributeStateWritePending,
  SmartstrapAttributeStateRequestPending,
  SmartstrapAttributeStateRequestInProgress,
  NumSmartstrapAttributeStates
} SmartstrapAttributeState;

typedef enum {
  SmartstrapRequestTypeRead,
  SmartstrapRequestTypeBeginWrite,
  SmartstrapRequestTypeWrite,
  SmartstrapRequestTypeWriteRead
} SmartstrapRequestType;


//! Initializes the smartstrap attribute code
void smartstrap_attribute_init(void);

//! Sends the next pending attribute request
bool smartstrap_attribute_send_pending(void);

//! Called by one of the profiles to send an event for an attribute.
void smartstrap_attribute_send_event(SmartstrapEventType type, SmartstrapProfile profile,
                                     SmartstrapResult result, uint16_t service_id,
                                     uint16_t attribute_id, uint16_t read_length);

//! Unregisters all attributes which the app has registered
void smartstrap_attribute_unregister_all(void);

// syscalls

//! Registers a new attribute by creating a kernel-space struct to represent it
bool sys_smartstrap_attribute_register(uint16_t service_id, uint16_t attribute_id, uint8_t *buffer,
                                       size_t buffer_length);

//! Unregisters an attribute
void sys_smartstrap_attribute_unregister(SmartstrapAttribute *app_attr);

//! Gets information on the specified attribute which has previously been created
void sys_smartstrap_attribute_get_info(SmartstrapAttribute *app_attr, uint16_t *service_id,
                                       uint16_t *attribute_id, size_t *length);

//! Queues up a request for the specified attribute
SmartstrapResult sys_smartstrap_attribute_do_request(SmartstrapAttribute *app_attr,
                                                     SmartstrapRequestType type,
                                                     uint16_t timeout_ms, uint32_t write_length);

//! Called by app_smartstrap.c after the app's event callback is called for an attribute
void sys_smartstrap_attribute_event_processed(SmartstrapAttribute *app_attr);
