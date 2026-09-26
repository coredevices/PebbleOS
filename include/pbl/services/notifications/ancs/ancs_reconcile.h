/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "comm/ble/kernel_le_client/ancs/ancs_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

//! Brings the watch's iOS notifications back in line with Notification Center after a
//! reconnection, using the notifications iOS replays when the watch subscribes to ANCS.

//! A notification iOS still has, identified by its content rather than only its UID
typedef struct {
  uint32_t uid;
  uint32_t app_id_hash;
  //! 0 when iOS sent no valid date
  time_t timestamp;
} ANCSReconcileEntry;

//! Builds an entry from the app identifier and date iOS returned for a notification
void ancs_reconcile_entry_from_attributes(uint32_t uid, const ANCSAttribute *app_id,
                                          const ANCSAttribute *date, ANCSReconcileEntry *entry_out);

//! @return whether the watch has any active iOS notification to compare
bool ancs_reconcile_has_candidates(void);

//! Finds the newest active iOS notification on the watch whose UID is in `uids`
//! @return false if there is none
bool ancs_reconcile_find_canary(const uint32_t *uids, size_t num_uids,
                                ANCSReconcileEntry *canary_out);

//! @return whether iOS's current entry for the canary's UID is still the same notification
bool ancs_reconcile_canary_matches(const ANCSReconcileEntry *expected,
                                   const ANCSReconcileEntry *fetched);

//! UIDs are unchanged: dismisses the active iOS notifications whose UID iOS no longer has.
//! Notifications with a UID in `live_uids` arrived since reconnecting and are always kept.
void ancs_reconcile_by_uid(const uint32_t *uids, size_t num_uids, const uint32_t *live_uids,
                           size_t num_live_uids);

//! UIDs changed: matches notifications on app and timestamp, dismisses the ones iOS no longer has
//! and moves the others to their new UIDs.
void ancs_reconcile_by_content(const ANCSReconcileEntry *entries, size_t num_entries,
                               const uint32_t *live_uids, size_t num_live_uids);
