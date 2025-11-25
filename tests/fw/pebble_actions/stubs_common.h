/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

﻿#pragma once

// These are the stubs that are common between the ancs_pebble_action and timeline_action tests
// This huge list is mainly due to the inclusion of timeline_actions.c which handles both UI and
// a large portion of action logic, which will hopefully be fixed eventually

#include "applib/ui/action_menu_window_private.h"
#include "services/normal/blob_db/ios_notif_pref_db.h"
#include "services/normal/blob_db/sync.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/phone_call_util.h"
#include "services/normal/timeline/actions_endpoint.h"
#include "services/normal/timeline/timeline.h"
#include "services/normal/timeline/timeline_actions.h"
#include "util/size.h"

#include "stubs_action_chaining_window.h"
#include "stubs_action_menu.h"
#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_manager.h"
#include "stubs_app_state.h"
#include "stubs_blob_db_sync.h"
#include "stubs_dialog.h"
#include "stubs_event_service_client.h"
#include "stubs_evented_timer.h"
#include "stubs_events.h"
#include "stubs_expandable_dialog.h"
#include "stubs_gcolor.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_notification_storage.h"
#include "stubs_notifications.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_pin_db.h"
#include "stubs_progress_window.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"
#include "stubs_reminder_db.h"
#include "stubs_rtc.h"
#include "stubs_simple_dialog.h"
#include "stubs_task_watchdog.h"
#include "stubs_ui_window.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

PebblePhoneCaller* phone_call_util_create_caller(const char *number, const char *name) {
  return NULL;
}

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
}

void system_task_add_callback(void (*callback)(void *data), void *data) {
}

void ancs_perform_action(uint32_t notification_uid, uint8_t action_id) {
}

status_t blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len) {
  return S_SUCCESS;
}

void timeline_pin_window_push_modal(TimelineItem *item) {
}

CommSession *comm_session_get_system_session(void) {
  // This can't be NULL (in that case we don't try to send the message)
  return (CommSession *) 1;
}

void comm_session_set_responsiveness(
    CommSession *session, BtConsumer consumer, ResponseTimeState state, uint16_t max_period_secs) {
  return;
}

void* event_service_claim_buffer(PebbleEvent *e) {
  return NULL;
}

void event_service_free_claimed_buffer(void *ref) {
  return;
}

void timeline_resources_get_id(const TimelineResourceInfo *timeline_res, TimelineResourceSize size,
                               AppResourceInfo *res_info) {
  return;
}

bool notification_window_is_modal(void) {
  return false;
}

size_t string_list_count(StringList *list) {
  return 0;
}

char *string_list_get_at(StringList *list, size_t index) {
  return NULL;
}

bool alerts_preferences_check_and_set_first_use_complete(int source) {
  return true;
}

static TimelineItemActionSource s_current_timeline_action_source =
    TimelineItemActionSourceModalNotification;

TimelineItemActionSource kernel_ui_get_current_timeline_item_action_source(void) {
  return s_current_timeline_action_source;
}

void kernel_ui_set_current_timeline_item_action_source(TimelineItemActionSource current_source) {
  s_current_timeline_action_source = current_source;
}
