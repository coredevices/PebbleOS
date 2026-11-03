/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <pbl/services/blob_db/pin_db.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/timeline/timeline.h>
#include <pbl/util/uuid.h>

#include <clar.h>

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include <fake_session.h>
#include <fake_settings_file.h>
#include <fake_system_task.h>

// Stubs
////////////////////////////////////////////////////////////////

const char *timeline_get_private_data_source(Uuid *parent_id) {
  return nullptr;
}

static TimelineItem item1 = {
  .header = {
    .id =
        {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
         0xb4},
    .parent_id =
        {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
         0x01},
    .timestamp = 1,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
    // don't care about the rest
  }
};

static TimelineItem item2 = {
  .header = {
    .id =
        {0x55, 0xcb, 0x7c, 0x75, 0x8a, 0x35, 0x44, 0x87, 0x90, 0xa4, 0x91, 0x3f, 0x1f, 0xa6, 0x76,
         0x01},
    .parent_id =
        {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
         0x01},
    .timestamp = 3,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  },
};

static TimelineItem item3 = {
  .header = {
    .id =
        {0x7c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c, 0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2,
         0x05},
    .parent_id =
        {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
         0x02},
    .timestamp = 4,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  }
};

static TimelineItem item4 = {
  .header = {
    .id =
        {0x8c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c, 0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2,
         0x05},
    .parent_id =
        {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22,
         0x03},
    .timestamp = 4,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  }
};

static TimelineItem reminder_app_item = {
  .header = {
    .id =
        {0x9c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c, 0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2,
         0x05},
    .parent_id = UUID_REMINDERS_DATA_SOURCE,
    .timestamp = 4,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  }
};

// Setup
////////////////////////////////////////////////////////////////

void test_pin_db__initialize(void) {
  pin_db_init();
}

void test_pin_db__cleanup(void) {
  pin_db_deinit();
  fake_settings_file_reset();
}

// Tests
////////////////////////////////////////////////////////////////

void test_pin_db__is_dirty_insert_from_phone(void) {
  // Insert a bunch of pins "from the phone"
  // They should NOT be dirty (the phone is the source of truth)
  pin_db_insert((uint8_t *)&item1.header.id, sizeof(TimelineItemId), (uint8_t *)&item1,
                sizeof(TimelineItem));
  pin_db_insert((uint8_t *)&item2.header.id, sizeof(TimelineItemId), (uint8_t *)&item2,
                sizeof(TimelineItem));
  pin_db_insert((uint8_t *)&item3.header.id, sizeof(TimelineItemId), (uint8_t *)&item3,
                sizeof(TimelineItem));
  pin_db_insert((uint8_t *)&item4.header.id, sizeof(TimelineItemId), (uint8_t *)&item4,
                sizeof(TimelineItem));

  bool is_dirty = true;
  cl_assert_equal_i(pin_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(!is_dirty);

  BlobDBDirtyItem *dirty_list = pin_db_get_dirty_list();
  cl_assert(!dirty_list);
}

void test_pin_db__is_dirty_insert_locally(void) {
  // Insert a bunch of pins "from the watch"
  // These should not be dirty because they are not from the reminders app
  pin_db_insert_item(&item1);
  pin_db_insert_item(&item2);
  pin_db_insert_item(&item3);
  pin_db_insert_item(&item4);

  bool is_dirty = true;
  cl_assert_equal_i(pin_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(!is_dirty);

  BlobDBDirtyItem *dirty_list = pin_db_get_dirty_list();
  cl_assert(!dirty_list);

  pin_db_insert_item(&reminder_app_item);
  cl_assert_equal_i(pin_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(is_dirty);

  dirty_list = pin_db_get_dirty_list();
  cl_assert(dirty_list);
  cl_assert(list_count((ListNode *)dirty_list) == 1);

  // Mark the reminder item as synced
  pin_db_mark_synced((uint8_t *)&reminder_app_item.header.id, sizeof(TimelineItemId));

  // And nothing should be dirty
  cl_assert_equal_i(pin_db_is_dirty(&is_dirty), S_SUCCESS);
  cl_assert(!is_dirty);

  dirty_list = pin_db_get_dirty_list();
  cl_assert(!dirty_list);
}

void test_pin_db__set_status_bits(void) {
  pin_db_insert_item(&item1);
  TimelineItem item;
  cl_must_pass(pin_db_read_item_header(&item, &item1.header.id));
  cl_assert_equal_i(item.header.status, 0);

  cl_must_pass(pin_db_set_status_bits(&item1.header.id, TimelineItemStatusDismissed));
  cl_must_pass(pin_db_read_item_header(&item, &item1.header.id));
  cl_assert_equal_i(item.header.status, TimelineItemStatusDismissed);
}

void test_pin_db__flush_keeps_watch_pins(void) {
  TimelineItem watch_item = item2;
  watch_item.header.from_watch = true;

  cl_must_pass(pin_db_insert_item(&item1));
  cl_must_pass(pin_db_insert_item(&watch_item));
  cl_must_pass(pin_db_insert_item(&item3));

  cl_must_pass(pin_db_flush());

  cl_assert_equal_i(pin_db_get_len((uint8_t *)&item1.header.id, sizeof(TimelineItemId)), 0);
  cl_assert_equal_i(pin_db_get_len((uint8_t *)&item3.header.id, sizeof(TimelineItemId)), 0);

  TimelineItem item;
  cl_must_pass(pin_db_read_item_header(&item, &watch_item.header.id));
  cl_assert(item.header.from_watch);
}

// 64-bit times
////////////////////////////////////////////////////////////////

// 2065, past what a signed 32-bit time holds
#define TIME_PAST_2038 ((time_t)3000000000LL)

//! item1, serialized as a phone without 64-bit times sends it
static size_t prv_serialize_32_bit(uint8_t *buf, time_t timestamp) {
  TimelineItem item = item1;
  item.header.timestamp = timestamp;
  SerializedTimelineItemHeader hdr;
  timeline_item_serialize_header(&item, &hdr);
  memcpy(buf, &hdr, sizeof(hdr));
  return timeline_item_serialized_narrow(buf, sizeof(hdr));
}

void test_pin_db__phone_without_time64(void) {
  uint8_t v1[sizeof(SerializedTimelineItemHeader)];
  const size_t v1_len = prv_serialize_32_bit(v1, TIME_PAST_2038);
  cl_assert_equal_i(v1_len, TIMELINE_ITEM_HEADER_TIME32_SIZE);

  cl_must_pass(pin_db_insert((uint8_t *)&item1.header.id, sizeof(TimelineItemId), v1, v1_len));

  // Stored, and read on the watch, with the full time
  TimelineItem item;
  cl_must_pass(pin_db_read_item_header(&item, &item1.header.id));
  cl_assert(item.header.timestamp == TIME_PAST_2038);
  cl_assert(uuid_equal(&item.header.parent_id, &item1.header.parent_id));
  cl_assert_equal_i(item.header.layout, LayoutIdTest);

  // Read back for the phone in the layout it sent
  cl_assert_equal_i(pin_db_get_len((uint8_t *)&item1.header.id, sizeof(TimelineItemId)), v1_len);
  uint8_t out[sizeof(SerializedTimelineItemHeader)];
  cl_must_pass(pin_db_read((uint8_t *)&item1.header.id, sizeof(TimelineItemId), out, v1_len));
  cl_assert_equal_m(out, v1, v1_len);
}

void test_pin_db__phone_with_time64(void) {
  Transport *transport = fake_transport_create(TransportDestinationSystem, nullptr, nullptr);
  fake_transport_set_connected(transport, true);
  cl_assert(timeline_item_phone_has_time64());

  TimelineItem src = item1;
  src.header.timestamp = (time_t)5000000000LL; // past 2106
  SerializedTimelineItemHeader hdr;
  timeline_item_serialize_header(&src, &hdr);
  cl_must_pass(pin_db_insert((uint8_t *)&item1.header.id, sizeof(TimelineItemId), (uint8_t *)&hdr,
                             sizeof(hdr)));

  TimelineItem item;
  cl_must_pass(pin_db_read_item_header(&item, &item1.header.id));
  cl_assert(item.header.timestamp == (time_t)5000000000LL);
  cl_assert_equal_i(pin_db_get_len((uint8_t *)&item1.header.id, sizeof(TimelineItemId)),
                    sizeof(hdr));

  fake_transport_set_connected(transport, false);
}
