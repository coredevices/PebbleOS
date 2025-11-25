/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/uuid.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/blob_db/pin_db.h"
#include "services/normal/timeline/attribute.h"
#include "services/normal/timeline/item.h"
#include "services/normal/timeline/timeline.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_rtc.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_manager.h"
#include "stubs_blob_db_sync.h"
#include "stubs_blob_db_sync_util.h"
#include "stubs_event_service_client.h"
#include "stubs_events.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"
#include "stubs_session.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_window_stack.h"


// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"

status_t blob_db_delete(BlobDBId db_id, const uint8_t *key, int key_len) {
  return pin_db_delete(key, key_len);
}

void ancs_notifications_enable_bulk_action_mode(bool enable) {
  return;
}

bool ancs_notifications_is_bulk_action_mode_enabled(void) {
  return false;
}

status_t reminder_db_delete_with_parent(const TimelineItemId *id) {
  return S_SUCCESS;
}

void timeline_action_endpoint_invoke_action(const Uuid *id,
    uint8_t action_id, AttributeList *attributes) {
}

const PebbleProcessMd *timeline_get_app_info(void) {
  return NULL;
}

void launcher_task_add_callback(void *data) {
}

void timeline_pin_window_push_modal(TimelineItem *item) {
}

PebblePhoneCaller* phone_call_util_create_caller(const char *number, const char *name) {
  return NULL;
}

void ancs_perform_action(uint32_t notification_uid, uint8_t action_id) {
}

void notifications_handle_notification_action_result(
    PebbleSysNotificationActionResult *action_result) {
}

void notification_storage_set_status(const Uuid *id, uint8_t status) {
}

void calendar_handle_pin_change(void) {
}

void notifications_handle_notification_acted_upon(Uuid *notification_id) {
  return;
}

// Setup
/////////////////////////

void test_timeline_api__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pin_db_init();
}

// Tests
///////////////////////////
void test_timeline_api__item(void) {
  TimelineItem *item = timeline_item_create_with_attributes(30, 0, TimelineItemTypePin,
    LayoutIdTest, NULL, NULL);

  cl_assert_equal_i(item->header.layout, LayoutIdTest);
  cl_assert_equal_i(item->header.timestamp, 30);
  timeline_item_destroy(item);
}

void test_timeline_api__pin_two_items(void) {
  TimelineItem *item1 = timeline_item_create_with_attributes(30, 0, TimelineItemTypePin,
    LayoutIdTest, NULL, NULL);
  fake_rtc_increment_ticks(1);
  TimelineItem *item2 = timeline_item_create_with_attributes(40, 0, TimelineItemTypePin,
    LayoutIdTest, NULL, NULL);
  Uuid id1 = item1->header.id;
  Uuid id2 = item2->header.id;

  cl_assert(timeline_add(item1));
  cl_assert(timeline_add(item2));

  TimelineItem item_temp = {0};
  cl_assert_equal_i(pin_db_get(&id1, &item_temp), 0);
  cl_assert(uuid_equal(&item1->header.id, &item_temp.header.id));
  cl_assert_equal_i(pin_db_get(&id2, &item_temp), 0);
  cl_assert(uuid_equal(&item2->header.id, &item_temp.header.id));

  timeline_item_destroy(item1);
  timeline_item_destroy(item2);
}

void test_timeline_api__item_attributes(void) {
  AttributeList list = {0};
  attribute_list_add_cstring(&list, AttributeIdTitle, "title");
  attribute_list_add_cstring(&list, AttributeIdSubtitle, "subtitle");
  TimelineItem *item = timeline_item_create_with_attributes(0, 0, TimelineItemTypePin,
    LayoutIdTest, &list, NULL);
  attribute_list_destroy_list(&list);

  cl_assert_equal_s(attribute_get_string(&item->attr_list, AttributeIdTitle, "none"),
    "title");
  cl_assert_equal_s(attribute_get_string(&item->attr_list, AttributeIdSubtitle, "none"),
    "subtitle");
  timeline_item_destroy(item);
}

void test_timeline_api__item_pin_to_timeline(void) {
  TimelineItem *item = timeline_item_create_with_attributes(0, 0, TimelineItemTypePin,
    LayoutIdTest, NULL, NULL);
  Uuid id = item->header.id;
  cl_assert(!timeline_exists(&id));

  cl_assert(timeline_add(item));
  timeline_item_destroy(item);
  cl_assert(timeline_exists(&id));

  TimelineItem item_temp;
  cl_assert_equal_i(pin_db_get(&id, &item_temp), 0);
  cl_assert(uuid_equal(&id, &item_temp.header.id));

  cl_assert(timeline_remove(&id));
  cl_assert(!timeline_exists(&id));
  cl_assert_equal_i(pin_db_get(&id, &item_temp), -9);
}

void test_timeline_api__item_attributes_pin_to_timeline(void) {
  AttributeList list = {0};
  attribute_list_add_cstring(&list, AttributeIdTitle, "title");
  attribute_list_add_cstring(&list, AttributeIdSubtitle, "subtitle");
  TimelineItem *item = timeline_item_create_with_attributes(0, 0, TimelineItemTypePin,
    LayoutIdTest, &list, NULL);
  attribute_list_destroy_list(&list);
  Uuid id = item->header.id;
  cl_assert(!timeline_exists(&id));
  cl_assert_equal_s(attribute_get_string(&item->attr_list, AttributeIdTitle, "none"),
    "title");
  cl_assert_equal_s(attribute_get_string(&item->attr_list, AttributeIdSubtitle, "none"),
    "subtitle");

  cl_assert(timeline_add(item));
  timeline_item_destroy(item);

  cl_assert(timeline_exists(&id));
  TimelineItem item_temp;
  cl_assert_equal_i(pin_db_get(&id, &item_temp), 0);
  cl_assert(uuid_equal(&id, &item_temp.header.id));
  cl_assert_equal_s(attribute_get_string(&item_temp.attr_list, AttributeIdTitle, "none"),
    "title");
  cl_assert_equal_s(attribute_get_string(&item_temp.attr_list, AttributeIdSubtitle, "none"),
    "subtitle");

  cl_assert(timeline_remove(&id));
  cl_assert(!timeline_exists(&id));
  cl_assert_equal_i(pin_db_get(&id, &item_temp), -9);
}
