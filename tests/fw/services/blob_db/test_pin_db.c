/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "services/normal/blob_db/pin_db.h"
#include "services/normal/timeline/timeline.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_settings_file.h"
#include "fake_system_task.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_blob_db.h"
#include "stubs_blob_db_sync.h"
#include "stubs_bt_lock.h"
#include "stubs_events.h"
#include "stubs_hexdump.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"
#include "stubs_reminder_db.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

const char *timeline_get_private_data_source(Uuid *parent_id) {
  return NULL;
}

static TimelineItem item1 = {
  .header = {
    .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4},
    .parent_id = {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0x01},
    .timestamp = 1,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
    // don't care about the rest
  }
};

static TimelineItem item2 = {
  .header = {
    .id = {0x55, 0xcb, 0x7c, 0x75, 0x8a, 0x35, 0x44, 0x87,
             0x90, 0xa4, 0x91, 0x3f, 0x1f, 0xa6, 0x76, 0x01},
    .parent_id = {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0x01},
    .timestamp = 3,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  },
};

static TimelineItem item3 = {
  .header = {
    .id = {0x7c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c,
             0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2, 0x05},
    .parent_id = {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0x02},
    .timestamp = 4,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  }
};

static TimelineItem item4 = {
  .header = {
    .id = {0x8c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c,
             0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2, 0x05},
    .parent_id = {0xff, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0x03},
    .timestamp = 4,
    .duration = 0,
    .type = TimelineItemTypePin,
    .layout = LayoutIdTest,
  }
};

static TimelineItem reminder_app_item = {
  .header = {
    .id = {0x9c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c,
             0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2, 0x05},
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
  pin_db_insert((uint8_t *)&item1.header.id, sizeof(TimelineItemId),
                (uint8_t *)&item1, sizeof(TimelineItem));
  pin_db_insert((uint8_t *)&item2.header.id, sizeof(TimelineItemId),
                (uint8_t *)&item2, sizeof(TimelineItem));
  pin_db_insert((uint8_t *)&item3.header.id, sizeof(TimelineItemId),
                (uint8_t *)&item3, sizeof(TimelineItem));
  pin_db_insert((uint8_t *)&item4.header.id, sizeof(TimelineItemId),
                (uint8_t *)&item4, sizeof(TimelineItem));

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
