/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! @file ancs.h The session with the Apple Notification Center Service of the phone: it queues
//! the notifications the ANCS client reports, gets their attributes and hands them to the
//! notification services. KernelMain only.

typedef enum {
  ANCSClientStateIdle = 0,
  ANCSClientStateRequestedNotification,
  ANCSClientStatePerformingAction,
  ANCSClientStateRequestedApp,
  ANCSClientStateAliveCheck,
  ANCSClientStateRetrying,
} ANCSClientState;

//! Starts a session on the connection of the phone: looks for the service, and subscribes to
//! it when the phone has it.
void ancs_create(uint16_t conn_handle);

//! Ends the session.
void ancs_destroy(void);

//! The services of the phone changed: looks for the service again.
void ancs_handle_services_changed(void);

//! Performs an action on a notification, an enum pbl_bt_ancs_action. Any task.
void ancs_perform_action(uint32_t notification_uid, uint8_t action_id);
