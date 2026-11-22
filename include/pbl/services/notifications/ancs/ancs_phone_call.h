/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/ancs.h>

/**
 * @defgroup services_notifications_ancs_ancs_phone_call ANCS phone calls
 * @ingroup services_notifications_ancs
 * @brief Phone call events derived from ANCS notifications.
 * @{
 */

/**
 * @brief Post an incoming call event.
 *
 * @param uid ANCS UID of the incoming call notification.
 * @param notif_attributes Notification attributes; the title holds the caller name or number.
 */
void ancs_phone_call_handle_incoming(uint32_t uid,
                                     const struct pbl_bt_ancs_attr *const *notif_attributes);

/**
 * @brief Post a hide call event, in response to an ANCS removal.
 *
 * @param uid ANCS UID of the removed incoming call notification.
 */
void ancs_phone_call_handle_removed(uint32_t uid);

/**
 * @brief Check whether missed call notifications are being ignored.
 *
 * @return true within a few seconds of ancs_phone_call_temporarily_block_missed_calls(), so that
 *         declining a call does not produce a missed call notification.
 */
bool ancs_phone_call_should_ignore_missed_calls(void);

/**
 * @brief Ignore missed call notifications for a few seconds.
 *
 * Called when a call is dismissed from the phone UI.
 */
void ancs_phone_call_temporarily_block_missed_calls(void);

/** @} */
