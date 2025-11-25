/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/uuid.h"
#include "services/normal/filesystem/pfs.h"
#include "services/common/regular_timer.h"
#include "services/normal/blob_db/pin_db.h"
#include "services/normal/timeline/timeline.h"
#include "apps/system_apps/timeline/timeline_model.h"
#include "util/size.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_spi_flash.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"

static TimezoneInfo tz = {
  .tm_gmtoff = -8 * 60 * 60, // PST
};

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_manager.h"
#include "stubs_blob_db.h"
#include "stubs_blob_db_sync.h"
#include "stubs_blob_db_sync_util.h"
#include "stubs_calendar.h"
#include "stubs_event_service_client.h"
#include "stubs_events.h"
#include "stubs_fonts.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"
#include "stubs_resources.h"
#include "stubs_session.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_window_stack.h"

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

void launcher_task_add_callback(void (*callback)(void *data), void *data) {
}

void timeline_pin_window_push_modal(TimelineItem *item) {
}

const PebbleProcessMd *timeline_get_app_info(void) {
  return NULL;
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

void notifications_handle_notification_acted_upon(Uuid *notification_id) {
  return;
}

// Data
/////////////////////////
static TimelineItem s_items[] = {
  {
    .header = { // [0]
        .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
                 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb1},
        .parent_id = {0},
        .timestamp =  1421178061, // Tue Jan 13 11:41:01 2015 PST
        .duration = 1,
        .type = TimelineItemTypePin,
        .flags = 0,
        .layout = LayoutIdTest,
    },
    .attr_list = {
        .num_attributes = 0,
        .attributes = NULL,
    },
    .action_group = {
        .num_actions = 0,
        .actions = NULL,
    },
    .allocated_buffer = NULL,
  }, {
    .header = { // [1]
        .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
                 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb2},
        .parent_id = {0},
        .timestamp =  1421183642, // Tue Jan 13 13:14:02 2015 PST
        .duration = 10,
        .type = TimelineItemTypePin,
        .flags = 0,
        .layout = LayoutIdTest,
    },
    .attr_list = {
        .num_attributes = 0,
        .attributes = NULL,
    },
    .action_group = {
        .num_actions = 0,
        .actions = NULL,
    },
    .allocated_buffer = NULL,
  }, {
    .header = { // [2]
        .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
                 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb3},
        .parent_id = {0},
        .timestamp =  1421183642, // Tue Jan 13 13:14:02 2015 PST
        .duration = 2,
        .type = TimelineItemTypePin,
        .flags = 0,
        .layout = LayoutIdTest,
    },
    .attr_list = {
        .num_attributes = 0,
        .attributes = NULL,
    },
    .action_group = {
        .num_actions = 0,
        .actions = NULL,
    },
    .allocated_buffer = NULL,
  }, {
    .header = { // [3]
        .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
                 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4},
        .parent_id = {0},
        .timestamp =  1421183642, // Tue Jan 13 13:14:02 2015 PST
        .duration = 30,
        .type = TimelineItemTypePin,
        .flags = 0,
        .layout = LayoutIdTest,
    },
    .attr_list = {
        .num_attributes = 0,
        .attributes = NULL,
    },
    .action_group = {
        .num_actions = 0,
        .actions = NULL,
    },
    .allocated_buffer = NULL,
  }, {
    .header = { // [4]
        .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
                 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb5},
        .parent_id = {0},
        .timestamp =  1421178061, // Tue Jan 13 11:41:01 2015 PST
        .duration = 5,
        .type = TimelineItemTypePin,
        .flags = 0,
        .layout = LayoutIdTest,
    },
    .attr_list = {
        .num_attributes = 0,
        .attributes = NULL,
    },
    .action_group = {
        .num_actions = 0,
        .actions = NULL,
    },
    .allocated_buffer = NULL,
  }, {
    .header = { // [5]
        .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
                 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb6},
        .parent_id = {0},
        .timestamp =  1421183462, // Tue Jan 13 13:11:02 PST 2015
        .duration = 4,
        .type = TimelineItemTypePin,
        .flags = 0,
        .layout = LayoutIdTest,
    },
    .attr_list = {
        .num_attributes = 0,
        .attributes = NULL,
    },
    .action_group = {
        .num_actions = 0,
        .actions = NULL,
    },
    .allocated_buffer = NULL,
  }
};

// Setup
/////////////////////////

void test_timeline_model__initialize(void) {
  fake_spi_flash_init(0, 0x1000000);
  fake_rtc_init(0, 0);
  pfs_init(false);
  // Note: creating a settings file is going to result in one malloc for the FD name
  pin_db_init();
  time_util_update_timezone(&tz);
  fake_pbl_malloc_clear_tracking();
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_items); ++i) {
    cl_assert_equal_i(pin_db_insert_item(&s_items[i]), 0);
  }
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

void test_timeline_model__cleanup(void) {
}

// Tests
///////////////////////////

static int s_correct_order[] = {0, 4, 5, 2, 1, 3};

void test_timeline_model__future(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  timeline_model_init(first_time, &model);

  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[0]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[1]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(0));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(1));

  int new_idx;
  bool has_next;
  cl_assert(timeline_model_iter_next(&new_idx, &has_next));
  cl_assert(has_next);
  cl_assert_equal_i(new_idx, 2);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[0]].header.id,
      &timeline_model_get_iter_state(-1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[1]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(1));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(2));

  cl_assert(timeline_model_iter_next(&new_idx, &has_next));
  cl_assert(has_next);
  cl_assert_equal_i(new_idx, 3);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[1]].header.id,
      &timeline_model_get_iter_state(-1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[3]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(2));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(3));

  cl_assert(timeline_model_iter_next(&new_idx, &has_next));
  cl_assert(has_next);
  cl_assert_equal_i(new_idx, 4);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(-1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[3]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[4]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(3));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(4));

  cl_assert(timeline_model_iter_next(&new_idx, &has_next));
  cl_assert(has_next);
  cl_assert_equal_i(new_idx, 5);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[3]].header.id,
      &timeline_model_get_iter_state(-1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[4]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[5]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(4));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(5));

  cl_assert(timeline_model_iter_next(&new_idx, &has_next));
  cl_assert(!has_next);
  cl_assert_equal_i(timeline_model_get_num_items(), 1);
  cl_assert(uuid_equal(&s_items[s_correct_order[4]].header.id,
      &timeline_model_get_iter_state(-1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[5]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));

  cl_assert(!timeline_model_iter_next(&new_idx, &has_next));
}

void test_timeline_model__and_back(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  timeline_model_init(first_time, &model);

  cl_assert(timeline_model_iter_next(NULL, NULL));
  cl_assert(timeline_model_iter_next(NULL, NULL));
  cl_assert(timeline_model_iter_next(NULL, NULL));
  cl_assert(timeline_model_iter_next(NULL, NULL));
  cl_assert(timeline_model_iter_next(NULL, NULL));
  cl_assert(!timeline_model_iter_next(NULL, NULL));

  int new_idx;
  cl_assert(timeline_model_iter_prev(&new_idx, NULL));
  cl_assert_equal_i(new_idx, 4);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[4]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[5]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(4));

  cl_assert(timeline_model_iter_prev(&new_idx, NULL));
  cl_assert_equal_i(new_idx, 3);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[3]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[4]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[5]].header.id,
      &timeline_model_get_iter_state(2)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(3));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(4));
  cl_assert(timeline_model_get_iter_state(2) == timeline_model_get_iter_state_with_timeline_idx(5));

  cl_assert(timeline_model_iter_prev(&new_idx, NULL));
  cl_assert_equal_i(new_idx, 2);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[3]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[4]].header.id,
      &timeline_model_get_iter_state(2)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(2));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(3));
  cl_assert(timeline_model_get_iter_state(2) == timeline_model_get_iter_state_with_timeline_idx(4));

  cl_assert(timeline_model_iter_prev(&new_idx, NULL));
  cl_assert_equal_i(new_idx, 1);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[1]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[3]].header.id,
      &timeline_model_get_iter_state(2)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(1));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(2));
  cl_assert(timeline_model_get_iter_state(2) == timeline_model_get_iter_state_with_timeline_idx(3));

  cl_assert(timeline_model_iter_prev(&new_idx, NULL));
  cl_assert_equal_i(new_idx, 0);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[0]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[1]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(2)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(0));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(1));
  cl_assert(timeline_model_get_iter_state(2) == timeline_model_get_iter_state_with_timeline_idx(2));

  cl_assert(!timeline_model_iter_prev(&new_idx, NULL));
}

void test_timeline_model__graceful_delete_middle(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  timeline_model_init(first_time, &model);

  timeline_model_remove(&s_items[s_correct_order[1]].header.id);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[0]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(0));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(2));
}

void test_timeline_model__graceful_delete_first(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  timeline_model_init(first_time, &model);

  timeline_model_remove(&s_items[s_correct_order[0]].header.id);
  cl_assert_equal_i(timeline_model_get_num_items(), 2);
  cl_assert(uuid_equal(&s_items[s_correct_order[1]].header.id,
      &timeline_model_get_iter_state(0)->pin.header.id));
  cl_assert(uuid_equal(&s_items[s_correct_order[2]].header.id,
      &timeline_model_get_iter_state(1)->pin.header.id));
  cl_assert(timeline_model_get_iter_state(0) == timeline_model_get_iter_state_with_timeline_idx(1));
  cl_assert(timeline_model_get_iter_state(1) == timeline_model_get_iter_state_with_timeline_idx(2));
}

void test_timeline_model__graceful_delete_all(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  timeline_model_init(first_time, &model);

  for (int i = 0; i < ARRAY_LENGTH(s_items); i++) {
    timeline_model_remove(&s_items[i].header.id);
  }
  cl_assert_equal_i(timeline_model_get_num_items(), 0);
  cl_assert(!timeline_model_iter_next(NULL, NULL));
  cl_assert(!timeline_model_iter_prev(NULL, NULL));
}

void test_timeline_model__is_empty(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  timeline_model_init(first_time, &model);

  cl_assert(!timeline_model_is_empty());

  for (int i = 0; i < ARRAY_LENGTH(s_items); i++) {
    timeline_model_remove(&s_items[i].header.id);
  }

  cl_assert(timeline_model_is_empty());
}

void test_timeline_model__is_empty_immediate(void) {
  TimelineModel model = {0};
  model.direction = TimelineIterDirectionFuture;
  time_t first_time = 1421178000;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  for (int i = 0; i < ARRAY_LENGTH(s_items); i++) {
    pin_db_delete((uint8_t *)&s_items[i].header.id, sizeof(TimelineItemId));
  }

  timeline_model_init(first_time, &model);

  cl_assert(timeline_model_is_empty());
}
