/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/ancs/ancs_reconcile.h"

#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "pbl/services/notifications/ancs/ancs_notifications_util.h"
#include "pbl/services/notifications/notification_storage.h"
#include "pbl/services/notifications/notifications.h"
#include "pbl/services/timeline/attribute.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

typedef struct {
  Uuid id;
  uint32_t ancs_uid;
  time_t timestamp;
} Candidate;

typedef struct {
  Candidate *candidates;
  size_t count;
} CandidateList;

typedef struct {
  Uuid id;
  uint32_t uid;
} Rekey;

typedef struct {
  const Rekey *rekeys;
  size_t count;
} RekeyContext;

typedef struct {
  size_t next;
  size_t count;
  Uuid ids[];
} DismissContext;

static bool prv_uid_in(uint32_t uid, const uint32_t *uids, size_t num_uids) {
  for (size_t i = 0; i < num_uids; i++) {
    if (uids[i] == uid) {
      return true;
    }
  }
  return false;
}

// Active iOS notifications: from ANCS and not yet dismissed or acted on, which hides them
static bool prv_is_candidate(const SerializedTimelineItemHeader *header) {
  return header->common.ancs_notif && (header->common.type == TimelineItemTypeNotification) &&
         !(header->common.status &
           (TimelineItemStatusDismissed | TimelineItemStatusActioned | TimelineItemStatusDeleted));
}

static bool prv_count_candidate(void *data, SerializedTimelineItemHeader *header) {
  if (prv_is_candidate(header)) {
    ((CandidateList *)data)->count++;
  }
  return true;
}

static bool prv_add_candidate(void *data, SerializedTimelineItemHeader *header) {
  CandidateList *list = data;
  if (prv_is_candidate(header)) {
    list->candidates[list->count++] = (Candidate){
      .id = header->common.id,
      .ancs_uid = header->common.ancs_uid,
      .timestamp = header->common.timestamp,
    };
  }
  return true;
}

//! Caller frees `candidates`
static CandidateList prv_get_candidates(void) {
  CandidateList counted = {};
  notification_storage_iterate(prv_count_candidate, &counted);
  if (counted.count == 0) {
    return (CandidateList){};
  }

  CandidateList list = {
    .candidates = kernel_malloc_check(counted.count * sizeof(Candidate)),
  };
  notification_storage_iterate(prv_add_candidate, &list);
  return list;
}

static bool prv_get_app_id_hash(const Uuid *id, uint32_t *hash_out) {
  TimelineItem item;
  if (!notification_storage_get(id, &item)) {
    return false;
  }
  const char *app_id = attribute_get_string(&item.attr_list, AttributeIdiOSAppIdentifier, "");
  *hash_out = ancs_notifications_util_hash_app_id((const uint8_t *)app_id, strlen(app_id));
  timeline_item_free_allocated_buffer(&item);
  return true;
}

// Dismissing posts an event to KernelMain, which can only queue a few to itself, so dismiss one
// notification per callback
static void prv_dismiss_step(void *data) {
  DismissContext *ctx = data;
  if (ctx->next >= ctx->count) {
    kernel_free(ctx);
    return;
  }

  // Same as a removal iOS reports while connected
  const Uuid *id = &ctx->ids[ctx->next++];
  uint8_t status;
  if (notification_storage_get_status(id, &status)) {
    notification_storage_set_status(id, status | TimelineItemStatusDismissed);
    // The event frees the id
    Uuid *event_id = kernel_malloc_check(sizeof(Uuid));
    *event_id = *id;
    notifications_handle_notification_acted_upon(event_id);
  }
  launcher_task_add_callback(prv_dismiss_step, ctx);
}

//! Takes ownership of `ctx`
static void prv_dismiss_all(DismissContext *ctx) {
  if (ctx->count == 0) {
    kernel_free(ctx);
    return;
  }
  PBL_LOG_INFO("Dismissing %u notifications cleared on the phone while disconnected",
               (unsigned)ctx->count);
  prv_dismiss_step(ctx);
}

static DismissContext *prv_create_dismiss_context(size_t max_count) {
  DismissContext *ctx = kernel_malloc_check(sizeof(DismissContext) + max_count * sizeof(Uuid));
  *ctx = (DismissContext){};
  return ctx;
}

static void prv_rekey_cb(TimelineItem *notification, SerializedTimelineItemHeader *header,
                         void *data) {
  const RekeyContext *ctx = data;
  for (size_t i = 0; i < ctx->count; i++) {
    if (uuid_equal(&header->common.id, &ctx->rekeys[i].id)) {
      header->common.ancs_uid = ctx->rekeys[i].uid;
      notification->header.ancs_uid = ctx->rekeys[i].uid;
      return;
    }
  }
}

void ancs_reconcile_entry_from_attributes(uint32_t uid, const ANCSAttribute *app_id,
                                          const ANCSAttribute *date,
                                          ANCSReconcileEntry *entry_out) {
  *entry_out = (ANCSReconcileEntry){
    .uid = uid,
    .app_id_hash = app_id ? ancs_notifications_util_hash_app_id(app_id->value, app_id->length)
                          : ancs_notifications_util_hash_app_id(NULL, 0),
    .timestamp = date ? ancs_notifications_util_parse_timestamp(date) : 0,
  };
}

static bool prv_stop_at_candidate(void *data, SerializedTimelineItemHeader *header) {
  if (prv_is_candidate(header)) {
    *(bool *)data = true;
    return false;
  }
  return true;
}

bool ancs_reconcile_has_candidates(void) {
  bool found = false;
  notification_storage_iterate(prv_stop_at_candidate, &found);
  return found;
}

bool ancs_reconcile_find_canary(const uint32_t *uids, size_t num_uids,
                                ANCSReconcileEntry *canary_out) {
  CandidateList list = prv_get_candidates();
  const Candidate *newest = NULL;
  for (size_t i = 0; i < list.count; i++) {
    const Candidate *candidate = &list.candidates[i];
    if (prv_uid_in(candidate->ancs_uid, uids, num_uids) &&
        (!newest || candidate->timestamp >= newest->timestamp)) {
      newest = candidate;
    }
  }

  bool found = false;
  if (newest) {
    canary_out->uid = newest->ancs_uid;
    canary_out->timestamp = newest->timestamp;
    found = prv_get_app_id_hash(&newest->id, &canary_out->app_id_hash);
  }
  kernel_free(list.candidates);
  return found;
}

bool ancs_reconcile_canary_matches(const ANCSReconcileEntry *expected,
                                   const ANCSReconcileEntry *fetched) {
  // Without a date it can't be told apart from another notification of the same app
  return (fetched->timestamp != 0) && (fetched->timestamp == expected->timestamp) &&
         (fetched->app_id_hash == expected->app_id_hash);
}

void ancs_reconcile_by_uid(const uint32_t *uids, size_t num_uids, const uint32_t *live_uids,
                           size_t num_live_uids) {
  CandidateList list = prv_get_candidates();
  DismissContext *dismiss = prv_create_dismiss_context(list.count);
  for (size_t i = 0; i < list.count; i++) {
    const Candidate *candidate = &list.candidates[i];
    if (!prv_uid_in(candidate->ancs_uid, uids, num_uids) &&
        !prv_uid_in(candidate->ancs_uid, live_uids, num_live_uids)) {
      dismiss->ids[dismiss->count++] = candidate->id;
    }
  }
  kernel_free(list.candidates);
  prv_dismiss_all(dismiss);
}

static const ANCSReconcileEntry *prv_find_entry(const Candidate *candidate, uint32_t app_id_hash,
                                                const ANCSReconcileEntry *entries,
                                                size_t num_entries, bool *exact_out) {
  const ANCSReconcileEntry *undated = NULL;
  for (size_t i = 0; i < num_entries; i++) {
    const ANCSReconcileEntry *entry = &entries[i];
    if (entry->app_id_hash != app_id_hash) {
      continue;
    }
    if (entry->timestamp == candidate->timestamp) {
      *exact_out = true;
      return entry;
    }
    if (entry->timestamp == 0) {
      // Can't be matched by time, so it keeps every notification of its app
      undated = entry;
    }
  }
  *exact_out = false;
  return undated;
}

void ancs_reconcile_by_content(const ANCSReconcileEntry *entries, size_t num_entries,
                               const uint32_t *live_uids, size_t num_live_uids) {
  CandidateList list = prv_get_candidates();
  DismissContext *dismiss = prv_create_dismiss_context(list.count);
  Rekey *rekeys = list.count ? kernel_malloc_check(list.count * sizeof(Rekey)) : NULL;
  size_t num_rekeys = 0;

  for (size_t i = 0; i < list.count; i++) {
    const Candidate *candidate = &list.candidates[i];
    if (prv_uid_in(candidate->ancs_uid, live_uids, num_live_uids)) {
      continue;
    }

    uint32_t app_id_hash;
    if (!prv_get_app_id_hash(&candidate->id, &app_id_hash)) {
      continue;
    }

    bool exact;
    const ANCSReconcileEntry *entry =
        prv_find_entry(candidate, app_id_hash, entries, num_entries, &exact);
    if (!entry) {
      dismiss->ids[dismiss->count++] = candidate->id;
    } else if (exact && (entry->uid != candidate->ancs_uid)) {
      rekeys[num_rekeys++] = (Rekey){.id = candidate->id, .uid = entry->uid};
    }
  }
  kernel_free(list.candidates);

  if (num_rekeys) {
    // Later removals from the phone and actions from the watch use the new UIDs
    PBL_LOG_INFO("Moving %u notifications to their new ANCS UIDs", (unsigned)num_rekeys);
    RekeyContext ctx = {.rekeys = rekeys, .count = num_rekeys};
    notification_storage_rewrite(prv_rekey_cb, &ctx);
  }
  kernel_free(rekeys);
  prv_dismiss_all(dismiss);
}
