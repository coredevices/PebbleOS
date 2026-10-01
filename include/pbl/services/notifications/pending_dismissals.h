/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/notifications/ancs/ancs_reconcile.h"
#include "pbl/services/timeline/item.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//! Dismissals the watch couldn't send because the phone was disconnected when a notification was
//! dismissed. They are kept across reboots and sent once the phone is back: to the Pebble app when
//! it reconnects, or over ANCS once iOS has replayed Notification Center.

//! Queues the dismissal of a notification dismissed while the phone can't be reached
//! @return false if the notification isn't dismissed on the phone at all
bool pending_dismissals_add(const TimelineItem *notification, const TimelineItemAction *dismiss);

//! Sends the dismissals queued for the Pebble app. Call when it connects.
void pending_dismissals_send_to_app(void);

//! @return whether any dismissal is queued for iOS
bool pending_dismissals_has_ancs(void);

//! Finds the newest queued iOS dismissal whose UID iOS replayed, to check whether iOS kept its UIDs
//! when the watch has no notification of its own to check with
//! @return false if there is none
bool pending_dismissals_find_ancs_canary(const uint32_t *uids, size_t num_uids,
                                         ANCSReconcileEntry *canary_out);

typedef void (*PendingDismissalSendCallback)(uint32_t ancs_uid, uint8_t action_id);

//! UIDs are unchanged: sends the queued iOS dismissals whose UID iOS replayed. The others are
//! dropped, as iOS no longer has them.
void pending_dismissals_resolve_ancs_by_uid(const uint32_t *uids, size_t num_uids,
                                            PendingDismissalSendCallback send);

//! UIDs changed: sends each queued iOS dismissal to the replayed notification with the same app and
//! date. The others are dropped, as iOS no longer has them or they can't be told apart.
void pending_dismissals_resolve_ancs_by_content(const ANCSReconcileEntry *entries,
                                                size_t num_entries,
                                                PendingDismissalSendCallback send);
