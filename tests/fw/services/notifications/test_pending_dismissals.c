/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/comm_session/session.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/notifications/ancs/ancs_notifications_util.h"
#include "pbl/services/notifications/pending_dismissals.h"
#include "pbl/services/system_task.h"
#include "pbl/util/size.h"
#include "pbl/util/uuid.h"

#include <string.h>

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_rtc.h"
#include "fake_spi_flash.h"

// Notifications from the Notifications data source are dismissed on the phone
static const Uuid s_phone_source = {0x01};

const char *timeline_get_private_data_source(Uuid *parent_id) {
  return uuid_equal(parent_id, &s_phone_source) ? "Notifications" : NULL;
}

static bool s_app_connected;
CommSession *comm_session_get_system_session(void) {
  return s_app_connected ? (CommSession *)1 : NULL;
}

bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  cb(data);
  return true;
}

#define MAX_SENT 40
static Uuid s_sent_ids[MAX_SENT];
static uint8_t s_sent_action_ids[MAX_SENT];
static int s_num_sent;

void timeline_action_endpoint_invoke_action(const Uuid *id, TimelineItemActionType type,
                                            uint8_t action_id, const AttributeList *attributes,
                                            bool do_async) {
  cl_assert_equal_i(type, TimelineItemActionTypeDismiss);
  cl_assert(s_num_sent < MAX_SENT);
  s_sent_ids[s_num_sent] = *id;
  s_sent_action_ids[s_num_sent++] = action_id;
}

static uint32_t s_ancs_sent_uids[MAX_SENT];
static uint8_t s_ancs_sent_action_ids[MAX_SENT];
static int s_num_ancs_sent;

static void prv_ancs_send(uint32_t uid, uint8_t action_id) {
  cl_assert(s_num_ancs_sent < MAX_SENT);
  s_ancs_sent_uids[s_num_ancs_sent] = uid;
  s_ancs_sent_action_ids[s_num_ancs_sent++] = action_id;
}

// Test notifications
////////////////////////////////////////////////////////////////

#define ANCS_ACTION_ID 1

static uint32_t prv_hash(const char *app_id) {
  return ancs_notifications_util_hash_app_id((const uint8_t *)app_id, strlen(app_id));
}

//! Dismisses an iOS notification while disconnected
static Uuid prv_add_ancs(uint32_t uid, const char *app_id, time_t timestamp) {
  Attribute notif_attributes[] = {
    {.id = AttributeIdiOSAppIdentifier, .cstring = (char *)app_id},
  };
  Attribute action_attributes[] = {
    {.id = AttributeIdAncsAction, .uint8 = ANCS_ACTION_ID},
  };
  TimelineItemAction dismiss = {
    .type = TimelineItemActionTypeAncsNegative,
    .attr_list = {
      .num_attributes = ARRAY_LENGTH(action_attributes),
      .attributes = action_attributes
    },
  };
  TimelineItem item = {
    .header = {.ancs_uid = uid, .timestamp = timestamp, .ancs_notif = true},
    .attr_list = {.num_attributes = ARRAY_LENGTH(notif_attributes), .attributes = notif_attributes},
  };
  uuid_generate(&item.header.id);
  cl_assert(pending_dismissals_add(&item, &dismiss));
  return item.header.id;
}

//! Dismisses an Android notification while disconnected
static Uuid prv_add_app(uint8_t action_id) {
  TimelineItemAction dismiss = {.id = action_id, .type = TimelineItemActionTypeDismiss};
  TimelineItem item = {.header = {.parent_id = s_phone_source}};
  uuid_generate(&item.header.id);
  cl_assert(pending_dismissals_add(&item, &dismiss));
  return item.header.id;
}

#define NOW (1790000000)

void test_pending_dismissals__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false);
  fake_rtc_init(0, NOW);
  s_app_connected = false;
  s_num_sent = 0;
  s_num_ancs_sent = 0;
}

void test_pending_dismissals__cleanup(void) {
}

void test_pending_dismissals__only_phone_dismissals_are_queued(void) {
  // Dismissed on the watch only: nothing to send later
  TimelineItemAction dismiss = {.type = TimelineItemActionTypeDismiss};
  TimelineItem from_watch = {.header = {.parent_id = s_phone_source, .from_watch = true}};
  cl_assert(!pending_dismissals_add(&from_watch, &dismiss));
  TimelineItem local = {};
  cl_assert(!pending_dismissals_add(&local, &dismiss));

  // Not a dismissal
  TimelineItemAction reply = {.type = TimelineItemActionTypeResponse};
  TimelineItem notification = {.header = {.parent_id = s_phone_source}};
  cl_assert(!pending_dismissals_add(&notification, &reply));

  cl_assert(!pending_dismissals_has_ancs());
  s_app_connected = true;
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 0);
}

void test_pending_dismissals__send_to_app(void) {
  const Uuid first = prv_add_app(3);
  const Uuid second = prv_add_app(4);
  prv_add_ancs(10, "com.a", NOW);

  // Nothing is sent while disconnected
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 0);

  s_app_connected = true;
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 2);
  cl_assert(uuid_equal(&s_sent_ids[0], &first) || uuid_equal(&s_sent_ids[1], &first));
  cl_assert(uuid_equal(&s_sent_ids[0], &second) || uuid_equal(&s_sent_ids[1], &second));

  // Sent once only; the iOS one is left for ANCS
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 2);
  cl_assert(pending_dismissals_has_ancs());
}

void test_pending_dismissals__ancs_canary(void) {
  prv_add_ancs(10, "com.a", NOW - 20);
  prv_add_ancs(11, "com.b", NOW - 10);
  prv_add_ancs(12, "com.c", NOW);

  // UID 12 is the newest, but iOS didn't replay it
  const uint32_t uids[] = {10, 11};
  ANCSReconcileEntry canary;
  cl_assert(pending_dismissals_find_ancs_canary(uids, ARRAY_LENGTH(uids), &canary));
  cl_assert_equal_i(canary.uid, 11);
  cl_assert_equal_i(canary.app_id_hash, prv_hash("com.b"));
  cl_assert(canary.timestamp == NOW - 10);

  const uint32_t none[] = {99};
  cl_assert(!pending_dismissals_find_ancs_canary(none, ARRAY_LENGTH(none), &canary));
}

void test_pending_dismissals__resolve_by_uid(void) {
  prv_add_ancs(10, "com.a", NOW);
  prv_add_ancs(11, "com.b", NOW);
  prv_add_app(3);

  // iOS still has 10; 11 was cleared on the phone as well
  const uint32_t uids[] = {10, 20};
  pending_dismissals_resolve_ancs_by_uid(uids, ARRAY_LENGTH(uids), prv_ancs_send);
  cl_assert_equal_i(s_num_ancs_sent, 1);
  cl_assert_equal_i(s_ancs_sent_uids[0], 10);
  cl_assert_equal_i(s_ancs_sent_action_ids[0], ANCS_ACTION_ID);

  // Both resolved, the Pebble app one untouched
  cl_assert(!pending_dismissals_has_ancs());
  s_app_connected = true;
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 1);
}

void test_pending_dismissals__resolve_by_content(void) {
  prv_add_ancs(10, "com.a", NOW - 60);
  prv_add_ancs(11, "com.b", NOW - 30);
  prv_add_ancs(12, "com.c", NOW);
  prv_add_ancs(13, "com.d", 0);

  // After a UID reset: com.a is now 50, com.b only has an undated entry, com.c is gone, and
  // com.d has no date on either side
  const ANCSReconcileEntry entries[] = {
    {.uid = 50, .app_id_hash = prv_hash("com.a"), .timestamp = NOW - 60},
    {.uid = 51, .app_id_hash = prv_hash("com.b"), .timestamp = 0},
    {.uid = 52, .app_id_hash = prv_hash("com.c"), .timestamp = NOW - 1},
    {.uid = 53, .app_id_hash = prv_hash("com.d"), .timestamp = 0},
  };
  pending_dismissals_resolve_ancs_by_content(entries, ARRAY_LENGTH(entries), prv_ancs_send);

  // Only the exact match is dismissed; nothing is guessed
  cl_assert_equal_i(s_num_ancs_sent, 1);
  cl_assert_equal_i(s_ancs_sent_uids[0], 50);
  cl_assert(!pending_dismissals_has_ancs());
}

void test_pending_dismissals__oldest_dropped_when_full(void) {
  prv_add_ancs(1, "com.a", NOW);
  for (int i = 0; i < 32; i++) {
    fake_rtc_increment_time(1);
    prv_add_app(i);
  }

  // The first one queued made room for the last
  const uint32_t uids[] = {1};
  pending_dismissals_resolve_ancs_by_uid(uids, ARRAY_LENGTH(uids), prv_ancs_send);
  cl_assert_equal_i(s_num_ancs_sent, 0);

  s_app_connected = true;
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 32);
}

void test_pending_dismissals__expire(void) {
  prv_add_ancs(1, "com.a", NOW);
  prv_add_app(3);

  fake_rtc_increment_time(8 * 24 * 60 * 60);
  cl_assert(!pending_dismissals_has_ancs());
  s_app_connected = true;
  pending_dismissals_send_to_app();
  cl_assert_equal_i(s_num_sent, 0);
}
