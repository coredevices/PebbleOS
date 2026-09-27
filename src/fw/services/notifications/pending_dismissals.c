/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/notifications/pending_dismissals.h"

#include "kernel/pbl_malloc.h"
#include "pbl/drivers/rtc.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/comm_session/session.h"
#include "pbl/services/notifications/ancs/ancs_notifications_util.h"
#include "pbl/services/settings/settings_file.h"
#include "pbl/services/system_task.h"
#include "pbl/services/timeline/actions_endpoint.h"
#include "pbl/services/timeline/attribute.h"
#include "pbl/services/timeline/timeline.h"
#include "pbl/util/size.h"
#include "util/time/time.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_notifications, CONFIG_SERVICE_NOTIFICATIONS_LOG_LEVEL);

#define FILE_NAME "notifpending"
#define FILE_LEN  2048

// Dismissing more while disconnected drops the oldest
#define MAX_PENDING 32

// By then the phone has almost certainly dismissed it itself
#define MAX_AGE_SECONDS (7 * SECONDS_PER_DAY)

typedef enum {
  PendingDismissalKindANCS = 0,
  PendingDismissalKindApp,
} PendingDismissalKind;

typedef struct PBL_PACKED {
  uint8_t kind;
  //! The Pebble app action's type and ID, or the ANCS action ID
  uint8_t action_type;
  uint8_t action_id;
  //! Identify the notification on iOS
  uint32_t ancs_uid;
  uint32_t app_id_hash;
  int32_t timestamp;
  uint32_t queued_at;
} PendingDismissal;

typedef struct {
  Uuid id;
  PendingDismissal dismissal;
} PendingEntry;

typedef struct {
  PendingEntry entries[MAX_PENDING + 1];
  size_t count;
} PendingList;

static PBL_MUTEX_DEFINE(s_mutex);
static bool s_send_to_app_scheduled;

static bool prv_load_entry(SettingsFile *file, SettingsRecordInfo *info, void *context) {
  PendingList *list = context;
  if ((info->key_len != sizeof(Uuid)) || (info->val_len != sizeof(PendingDismissal)) ||
      (list->count >= ARRAY_LENGTH(list->entries))) {
    return true;
  }
  PendingEntry *entry = &list->entries[list->count++];
  info->get_key(file, &entry->id, sizeof(entry->id));
  info->get_val(file, &entry->dismissal, sizeof(entry->dismissal));
  return true;
}

//! Loads every entry, deleting the expired ones. Call with the file open and the mutex held.
static void prv_load(SettingsFile *file, PendingList *list) {
  list->count = 0;
  settings_file_each(file, prv_load_entry, list);

  const time_t now = rtc_get_time();
  size_t kept = 0;
  for (size_t i = 0; i < list->count; i++) {
    const PendingEntry *entry = &list->entries[i];
    if (now - (time_t)entry->dismissal.queued_at > MAX_AGE_SECONDS) {
      settings_file_delete(file, &entry->id, sizeof(entry->id));
    } else {
      list->entries[kept++] = *entry;
    }
  }
  list->count = kept;
}

static bool prv_open(SettingsFile *file) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  if (settings_file_open(file, FILE_NAME, FILE_LEN) != S_SUCCESS) {
    pbl_mutex_unlock(&s_mutex);
    return false;
  }
  return true;
}

static void prv_close(SettingsFile *file) {
  settings_file_close(file);
  pbl_mutex_unlock(&s_mutex);
}

// Local-only dismissals, like the ones of notifications created on the watch, never reach the phone
static bool prv_is_dismissed_on_phone(const TimelineItem *notification) {
  if (notification->header.from_watch) {
    return false;
  }
  Uuid parent_id = notification->header.parent_id;
  return timeline_get_private_data_source(&parent_id) != NULL;
}

static bool prv_make_dismissal(const TimelineItem *notification, const TimelineItemAction *dismiss,
                               PendingDismissal *out) {
  *out = (PendingDismissal){
    .timestamp = notification->header.timestamp,
    .queued_at = rtc_get_time(),
  };

  if ((dismiss->type == TimelineItemActionTypeAncsNegative) ||
      (dismiss->type == TimelineItemActionTypeAncsDelete)) {
    // Same UID and action as timeline_invoke_action() would use
    out->kind = PendingDismissalKindANCS;
    out->action_id =
        attribute_get_uint8(&dismiss->attr_list, AttributeIdAncsAction, TIMELINE_INVALID_ACTION_ID);
    out->ancs_uid =
        attribute_get_uint32(&dismiss->attr_list, AttributeIdAncsId, notification->header.ancs_uid);
    const char *app_id =
        attribute_get_string(&notification->attr_list, AttributeIdiOSAppIdentifier, "");
    out->app_id_hash = ancs_notifications_util_hash_app_id((const uint8_t *)app_id, strlen(app_id));
    return true;
  }

  if ((dismiss->type == TimelineItemActionTypeDismiss) && prv_is_dismissed_on_phone(notification)) {
    out->kind = PendingDismissalKindApp;
    out->action_type = dismiss->type;
    out->action_id = dismiss->id;
    return true;
  }

  return false;
}

bool pending_dismissals_add(const TimelineItem *notification, const TimelineItemAction *dismiss) {
  PendingDismissal dismissal;
  if (!prv_make_dismissal(notification, dismiss, &dismissal)) {
    return false;
  }

  SettingsFile file;
  if (!prv_open(&file)) {
    return false;
  }

  PendingList *list = kernel_malloc_check(sizeof(PendingList));
  prv_load(&file, list);
  if (list->count >= MAX_PENDING) {
    const PendingEntry *oldest = &list->entries[0];
    for (size_t i = 1; i < list->count; i++) {
      if (list->entries[i].dismissal.queued_at < oldest->dismissal.queued_at) {
        oldest = &list->entries[i];
      }
    }
    settings_file_delete(&file, &oldest->id, sizeof(oldest->id));
  }
  kernel_free(list);

  const status_t status =
      settings_file_set(&file, &notification->header.id, sizeof(notification->header.id),
                        &dismissal, sizeof(dismissal));
  prv_close(&file);

  PBL_LOG_INFO("Phone unreachable, dismissal queued until it reconnects");
  return (status == S_SUCCESS);
}

// Sends from the system task, like other Pebble app actions, and only removes what was sent
static void prv_send_to_app_system_task_cb(void *unused) {
  s_send_to_app_scheduled = false;

  SettingsFile file;
  if (!prv_open(&file)) {
    return;
  }
  PendingList *list = kernel_malloc_check(sizeof(PendingList));
  prv_load(&file, list);

  unsigned sent = 0;
  for (size_t i = 0; i < list->count; i++) {
    const PendingEntry *entry = &list->entries[i];
    if (entry->dismissal.kind != PendingDismissalKindApp) {
      continue;
    }
    if (!comm_session_get_system_session()) {
      // Disconnected again: the rest waits for the next connection
      break;
    }
    timeline_action_endpoint_invoke_action(&entry->id,
                                           (TimelineItemActionType)entry->dismissal.action_type,
                                           entry->dismissal.action_id, NULL, false /* do_async */);
    settings_file_delete(&file, &entry->id, sizeof(entry->id));
    sent++;
  }
  kernel_free(list);
  prv_close(&file);

  if (sent) {
    PBL_LOG_INFO("Sent %u dismissals queued while disconnected", sent);
  }
}

void pending_dismissals_send_to_app(void) {
  if (s_send_to_app_scheduled) {
    return;
  }
  // Set first: the system task may run the callback, which clears it, before this returns
  s_send_to_app_scheduled = true;
  if (!system_task_add_callback(prv_send_to_app_system_task_cb, NULL)) {
    s_send_to_app_scheduled = false;
  }
}

bool pending_dismissals_has_ancs(void) {
  SettingsFile file;
  if (!prv_open(&file)) {
    return false;
  }
  PendingList *list = kernel_malloc_check(sizeof(PendingList));
  prv_load(&file, list);
  bool found = false;
  for (size_t i = 0; i < list->count; i++) {
    if (list->entries[i].dismissal.kind == PendingDismissalKindANCS) {
      found = true;
      break;
    }
  }
  kernel_free(list);
  prv_close(&file);
  return found;
}

static bool prv_uid_in(uint32_t uid, const uint32_t *uids, size_t num_uids) {
  for (size_t i = 0; i < num_uids; i++) {
    if (uids[i] == uid) {
      return true;
    }
  }
  return false;
}

bool pending_dismissals_find_ancs_canary(const uint32_t *uids, size_t num_uids,
                                         ANCSReconcileEntry *canary_out) {
  SettingsFile file;
  if (!prv_open(&file)) {
    return false;
  }
  PendingList *list = kernel_malloc_check(sizeof(PendingList));
  prv_load(&file, list);

  const PendingDismissal *newest = NULL;
  for (size_t i = 0; i < list->count; i++) {
    const PendingDismissal *dismissal = &list->entries[i].dismissal;
    if ((dismissal->kind == PendingDismissalKindANCS) &&
        prv_uid_in(dismissal->ancs_uid, uids, num_uids) &&
        (!newest || (dismissal->timestamp >= newest->timestamp))) {
      newest = dismissal;
    }
  }
  if (newest) {
    *canary_out = (ANCSReconcileEntry){
      .uid = newest->ancs_uid,
      .app_id_hash = newest->app_id_hash,
      .timestamp = newest->timestamp,
    };
  }
  kernel_free(list);
  prv_close(&file);
  return (newest != NULL);
}

typedef struct {
  uint32_t uid;
  uint8_t action_id;
} ANCSSend;

//! Resolves every queued iOS dismissal with `match`, then sends outside the lock
static void prv_resolve_ancs(bool (*match)(const PendingDismissal *dismissal, const void *context,
                                           uint32_t *uid_out),
                             const void *context, PendingDismissalSendCallback send) {
  SettingsFile file;
  if (!prv_open(&file)) {
    return;
  }
  PendingList *list = kernel_malloc_check(sizeof(PendingList));
  prv_load(&file, list);

  ANCSSend sends[MAX_PENDING];
  size_t num_sends = 0;
  for (size_t i = 0; i < list->count; i++) {
    const PendingEntry *entry = &list->entries[i];
    if (entry->dismissal.kind != PendingDismissalKindANCS) {
      continue;
    }
    uint32_t uid;
    if (match(&entry->dismissal, context, &uid) && (num_sends < ARRAY_LENGTH(sends))) {
      sends[num_sends++] = (ANCSSend){.uid = uid, .action_id = entry->dismissal.action_id};
    }
    // Resolved either way: sent now, or iOS no longer has it
    settings_file_delete(&file, &entry->id, sizeof(entry->id));
  }
  kernel_free(list);
  prv_close(&file);

  if (num_sends) {
    PBL_LOG_INFO("Sending %u iOS dismissals queued while disconnected", (unsigned)num_sends);
  }
  for (size_t i = 0; i < num_sends; i++) {
    send(sends[i].uid, sends[i].action_id);
  }
}

typedef struct {
  const uint32_t *uids;
  size_t num_uids;
} UidContext;

static bool prv_match_by_uid(const PendingDismissal *dismissal, const void *context,
                             uint32_t *uid_out) {
  const UidContext *ctx = context;
  *uid_out = dismissal->ancs_uid;
  return prv_uid_in(dismissal->ancs_uid, ctx->uids, ctx->num_uids);
}

void pending_dismissals_resolve_ancs_by_uid(const uint32_t *uids, size_t num_uids,
                                            PendingDismissalSendCallback send) {
  const UidContext ctx = {.uids = uids, .num_uids = num_uids};
  prv_resolve_ancs(prv_match_by_uid, &ctx, send);
}

typedef struct {
  const ANCSReconcileEntry *entries;
  size_t num_entries;
} ContentContext;

// Only an exact app and date match: dismissing the wrong notification on the phone is worse than
// not dismissing one
static bool prv_match_by_content(const PendingDismissal *dismissal, const void *context,
                                 uint32_t *uid_out) {
  const ContentContext *ctx = context;
  for (size_t i = 0; i < ctx->num_entries; i++) {
    const ANCSReconcileEntry *entry = &ctx->entries[i];
    if ((entry->app_id_hash == dismissal->app_id_hash) && (entry->timestamp != 0) &&
        (entry->timestamp == dismissal->timestamp)) {
      *uid_out = entry->uid;
      return true;
    }
  }
  return false;
}

void pending_dismissals_resolve_ancs_by_content(const ANCSReconcileEntry *entries,
                                                size_t num_entries,
                                                PendingDismissalSendCallback send) {
  const ContentContext ctx = {.entries = entries, .num_entries = num_entries};
  prv_resolve_ancs(prv_match_by_content, &ctx, send);
}
