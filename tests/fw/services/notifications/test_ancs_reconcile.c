/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "kernel/pbl_malloc.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/notifications/ancs/ancs_notifications_util.h"
#include "pbl/services/notifications/ancs/ancs_reconcile.h"
#include "pbl/services/notifications/notification_storage.h"
#include "pbl/util/size.h"
#include "pbl/util/uuid.h"

#include <string.h>

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_event_loop.h"
#include "stubs_hexdump.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_serial.h"
#include "stubs_sleep.h"
#include "stubs_task_wdt.h"

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_rtc.h"
#include "fake_spi_flash.h"

extern void notification_storage_reset(void);

#define MAX_DISMISSED 8
static Uuid s_dismissed[MAX_DISMISSED];
static int s_num_dismissed;

void notifications_handle_notification_acted_upon(Uuid *notification_id) {
  cl_assert(s_num_dismissed < MAX_DISMISSED);
  s_dismissed[s_num_dismissed++] = *notification_id;
  // The event system frees the id
  kernel_free(notification_id);
}

static bool prv_was_dismissed(const Uuid *id) {
  for (int i = 0; i < s_num_dismissed; i++) {
    if (uuid_equal(&s_dismissed[i], id)) {
      return true;
    }
  }
  return false;
}

// ANCS attributes, laid out as iOS sends them
typedef struct PBL_PACKED {
  uint8_t id;
  uint16_t length;
  char value[40];
} TestAttribute;

static TestAttribute prv_attribute(NotificationAttributeID id, const char *value) {
  TestAttribute attr = {.id = id, .length = strlen(value)};
  memcpy(attr.value, value, attr.length);
  return attr;
}

static time_t prv_timestamp(const char *date) {
  TestAttribute attr = prv_attribute(NotificationAttributeIDDate, date);
  return ancs_notifications_util_parse_timestamp((ANCSAttribute *)&attr);
}

static ANCSReconcileEntry prv_entry(uint32_t uid, const char *app_id, const char *date) {
  TestAttribute app_attr = prv_attribute(NotificationAttributeIDAppIdentifier, app_id);
  TestAttribute date_attr = prv_attribute(NotificationAttributeIDDate, date ? date : "");
  ANCSReconcileEntry entry;
  ancs_reconcile_entry_from_attributes(uid, (ANCSAttribute *)&app_attr,
                                       date ? (ANCSAttribute *)&date_attr : NULL, &entry);
  return entry;
}

static Uuid prv_store(uint32_t ancs_uid, const char *app_id, const char *date, bool from_ancs,
                      uint8_t status) {
  Attribute attributes[] = {
    {.id = AttributeIdTitle, .cstring = "Title"},
    {.id = AttributeIdiOSAppIdentifier, .cstring = (char *)app_id},
  };
  TimelineItem item = {
    .header =
        {
          .ancs_uid = ancs_uid,
          .timestamp = prv_timestamp(date),
          .type = TimelineItemTypeNotification,
          .layout = LayoutIdNotification,
          .ancs_notif = from_ancs,
          .status = status,
        },
    .attr_list = {.num_attributes = ARRAY_LENGTH(attributes), .attributes = attributes},
  };
  uuid_generate(&item.header.id);
  notification_storage_store(&item);
  return item.header.id;
}

static uint8_t prv_status(const Uuid *id) {
  uint8_t status = 0;
  cl_assert(notification_storage_get_status(id, &status));
  return status;
}

static bool prv_is_dismissed(const Uuid *id) {
  return prv_status(id) & TimelineItemStatusDismissed;
}

static uint32_t prv_stored_uid(const Uuid *id) {
  TimelineItem item;
  cl_assert(notification_storage_get(id, &item));
  const uint32_t uid = item.header.ancs_uid;
  timeline_item_free_allocated_buffer(&item);
  return uid;
}

#define DATE_A "20260925T101500"
#define DATE_B "20260925T111500"
#define DATE_C "20260925T121500"

void test_ancs_reconcile__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(false /* write erase headers */);
  notification_storage_reset();
  s_num_dismissed = 0;
}

void test_ancs_reconcile__cleanup(void) {
}

void test_ancs_reconcile__find_canary_picks_newest_replayed(void) {
  prv_store(1, "com.a", DATE_A, true, 0);
  prv_store(2, "com.b", DATE_C, true, 0);
  prv_store(3, "com.c", DATE_B, true, 0);

  // UID 2 is the newest, but iOS didn't replay it
  const uint32_t uids[] = {1, 3};
  ANCSReconcileEntry canary;
  cl_assert(ancs_reconcile_find_canary(uids, ARRAY_LENGTH(uids), &canary));
  cl_assert_equal_i(canary.uid, 3);
  cl_assert(canary.timestamp == prv_timestamp(DATE_B));

  const ANCSReconcileEntry same = prv_entry(3, "com.c", DATE_B);
  cl_assert(ancs_reconcile_canary_matches(&canary, &same));
  const ANCSReconcileEntry other_app = prv_entry(3, "com.a", DATE_B);
  cl_assert(!ancs_reconcile_canary_matches(&canary, &other_app));
  const ANCSReconcileEntry other_time = prv_entry(3, "com.c", DATE_A);
  cl_assert(!ancs_reconcile_canary_matches(&canary, &other_time));
  const ANCSReconcileEntry undated = prv_entry(3, "com.c", NULL);
  cl_assert(!ancs_reconcile_canary_matches(&canary, &undated));
}

void test_ancs_reconcile__find_canary_none(void) {
  prv_store(1, "com.a", DATE_A, true, 0);
  // Dismissed and non-iOS notifications are never candidates
  prv_store(2, "com.b", DATE_B, true, TimelineItemStatusDismissed);
  prv_store(3, "com.c", DATE_C, false, 0);

  const uint32_t uids[] = {2, 3};
  ANCSReconcileEntry canary;
  cl_assert(!ancs_reconcile_find_canary(uids, ARRAY_LENGTH(uids), &canary));
}

void test_ancs_reconcile__by_uid(void) {
  const Uuid kept = prv_store(1, "com.a", DATE_A, true, 0);
  const Uuid cleared = prv_store(2, "com.b", DATE_B, true, TimelineItemStatusRead);
  const Uuid live = prv_store(9, "com.c", DATE_C, true, 0);
  // Already hidden: left as they are
  const Uuid dismissed = prv_store(3, "com.d", DATE_C, true, TimelineItemStatusDismissed);
  const Uuid actioned = prv_store(5, "com.f", DATE_C, true, TimelineItemStatusActioned);
  const Uuid not_ios = prv_store(4, "com.e", DATE_C, false, 0);

  const uint32_t uids[] = {1};
  const uint32_t live_uids[] = {9};
  ancs_reconcile_by_uid(uids, ARRAY_LENGTH(uids), live_uids, ARRAY_LENGTH(live_uids));

  cl_assert_equal_i(s_num_dismissed, 1);
  cl_assert(prv_was_dismissed(&cleared));
  // Dismissed like a removal iOS reports live, keeping the other flags
  cl_assert_equal_i(prv_status(&cleared), TimelineItemStatusRead | TimelineItemStatusDismissed);
  cl_assert(!prv_is_dismissed(&kept));
  cl_assert(!prv_is_dismissed(&live));
  cl_assert_equal_i(prv_status(&dismissed), TimelineItemStatusDismissed);
  cl_assert_equal_i(prv_status(&actioned), TimelineItemStatusActioned);
  cl_assert(!prv_is_dismissed(&not_ios));
}

void test_ancs_reconcile__by_content_dismisses_and_rekeys(void) {
  const Uuid moved = prv_store(1, "com.a", DATE_A, true, 0);
  const Uuid cleared = prv_store(2, "com.b", DATE_B, true, 0);
  const Uuid same_app_other_time = prv_store(3, "com.a", DATE_C, true, 0);
  const Uuid live = prv_store(9, "com.z", DATE_C, true, 0);

  // After a UID reset iOS now calls the com.a notification 50
  const ANCSReconcileEntry entries[] = {
    prv_entry(50, "com.a", DATE_A),
    prv_entry(51, "com.x", DATE_B),
  };
  const uint32_t live_uids[] = {9};
  ancs_reconcile_by_content(entries, ARRAY_LENGTH(entries), live_uids, ARRAY_LENGTH(live_uids));

  cl_assert_equal_i(s_num_dismissed, 2);
  cl_assert(prv_was_dismissed(&cleared));
  cl_assert(prv_was_dismissed(&same_app_other_time));
  cl_assert(prv_is_dismissed(&cleared));
  cl_assert(prv_is_dismissed(&same_app_other_time));

  cl_assert(!prv_is_dismissed(&moved));
  cl_assert_equal_i(prv_stored_uid(&moved), 50);
  cl_assert(!prv_is_dismissed(&live));
  cl_assert_equal_i(prv_stored_uid(&live), 9);
}

void test_ancs_reconcile__by_content_undated_keeps_app(void) {
  const Uuid first = prv_store(1, "com.a", DATE_A, true, 0);
  const Uuid second = prv_store(2, "com.a", DATE_B, true, 0);
  const Uuid other = prv_store(3, "com.b", DATE_B, true, 0);

  // iOS sent no usable date, so every com.a notification may still be there
  const ANCSReconcileEntry entries[] = {prv_entry(60, "com.a", NULL)};
  ancs_reconcile_by_content(entries, ARRAY_LENGTH(entries), NULL, 0);

  cl_assert_equal_i(s_num_dismissed, 1);
  cl_assert(prv_was_dismissed(&other));
  cl_assert(!prv_is_dismissed(&first));
  cl_assert(!prv_is_dismissed(&second));
  // Not matched exactly, so the UIDs are left alone
  cl_assert_equal_i(prv_stored_uid(&first), 1);
}

void test_ancs_reconcile__nothing_to_do(void) {
  ancs_reconcile_by_uid(NULL, 0, NULL, 0);
  ancs_reconcile_by_content(NULL, 0, NULL, 0);
  cl_assert_equal_i(s_num_dismissed, 0);
}
