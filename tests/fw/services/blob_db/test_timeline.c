/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "util/uuid.h"
#include "services/normal/filesystem/pfs.h"
#include "services/common/regular_timer.h"
#include "services/normal/blob_db/pin_db.h"
#include "services/normal/timeline/timeline.h"

#include "util/list.h"
#include "util/size.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"
#include "fake_settings_file.h"

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
#include "stubs_session.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"
#include "stubs_window_stack.h"

struct TimelineNode {
  ListNode node;
  int index;
  Uuid id;
  time_t timestamp;
  uint16_t duration;
  bool all_day;
};

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

// items with long duration
static TimelineItem s_long_items[] = {
  {
    .header = {
      .id = {0xaa},
      .timestamp = 10000,
      .duration = 30,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  },
  {
    .header = {
      .id = {0xbb},
      .timestamp = 12000,
      .duration = 30,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  },
  {
    .header = {
      .id = {0xcc},
      .timestamp = 14000,
      .duration = 30,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  },
  {
    .header = {
      .id = {0xdd},
      .timestamp = 16000,
      .duration = 30,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  },
  {
    .header = {
      .id = {0xee},
      .timestamp = 18000,
      .duration = 30,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  }
};

// all day item
static TimelineItem s_all_day_items[] = {
  {
    .header = {
        .id = {0x01},
        .parent_id = {0},
        .timestamp =  1421020800, // midnight jan 12, 2015 UTC
        .duration = MINUTES_PER_DAY,
        .type = TimelineItemTypePin,
        .all_day = 1,
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
    .header = {
        .id = {0x02},
        .parent_id = {0},
        .timestamp =  1421107200, // Tue Jan 13 midnight 2015 UTC
        .duration = MINUTES_PER_DAY,
        .type = TimelineItemTypePin,
        .all_day = 1,
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
    .header = {
        .id = {0x03},
        .parent_id = {0},
        .timestamp =  1421107200, // Tue Jan 13 midnight 2015 UTC
        .duration = MINUTES_PER_DAY,
        .type = TimelineItemTypePin,
        .all_day = 1,
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

static const int s_feb_5_midnight = 1423123200; // 2015 PST
static const int s_feb_5_midnight_utc = 1423094400; // 2015 UTC

// extra case -- one all day event, one event from 8:00 to 10:00, and one from 8:15-8:16
// should cover most edge cases since it deals with all day events and skipping events
// that have past by endtime
static TimelineItem s_extra_case_items[] = {
  {
    .header = {
      .id = {0xbb},
      .parent_id = {0},
      .timestamp = s_feb_5_midnight_utc,
      .duration = MINUTES_PER_DAY,
      .type = TimelineItemTypePin,
      .all_day = 1,
      .layout = LayoutIdTest,
    },
  }, {
    .header = {
      .id = {0xcc},
      .parent_id = {0},
      .timestamp = s_feb_5_midnight + 8 * 60 * 60, // 8:00-10:00 am
      .duration = 120,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  }, {
    .header = {
      .id = {0xdd},
      .parent_id = {0},
      .timestamp = s_feb_5_midnight + 8 * 60 * 60 + 15 * 60, // 8:15-8:16 am
      .duration = 1,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  }
};

// Setup
/////////////////////////

void test_timeline__initialize(void) {
  fake_rtc_init(0, 0);
  // Note: creating a settings file is going to result in one malloc for the FD name
  pin_db_init();
  time_util_update_timezone(&tz);
  uint8_t num_net_allocs = fake_pbl_malloc_num_net_allocs();
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_items); ++i) {
    cl_assert_equal_i(pin_db_insert_item(&s_items[i]), 0);
  }
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), num_net_allocs);
}

void test_timeline__cleanup(void) {
  fake_settings_file_reset();
  fake_pbl_malloc_clear_tracking();
}

// Tests
///////////////////////////
void test_timeline__all_forwards(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  // check first
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
   1421178000), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  // check second
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check third
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[5].header.id));
  // check second again
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check fourth
  cl_assert(iter_next(&iterator));
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[2].header.id));
  // check fifth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  // check sixth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));

  // check rollover behaviour
  cl_assert(!iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));
  cl_assert(state.node);

  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
}

void test_timeline__forward_and_back(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // Note: 1421178000 = Tue Jan 13 11:40:00 PST 2015
  // check first
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
   1421178000), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  // check second
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check first again
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));

  cl_assert(!iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  cl_assert(state.node);
}

void test_timeline__none_forwards(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    1421188000), 2);
}

void test_timeline__all_backwards(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // Note: 1421188000 == Tue Jan 13 14:26:40 PST 2015
  // check first
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    1421188000), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));
  // check second
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  // check third
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[2].header.id));
  // check fourth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[5].header.id));
  // check third again
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[2].header.id));
  // check fifth
  cl_assert(iter_next(&iterator));
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check sixth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
}

void test_timeline__none_backwards(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    1421178000), 2);
}

void test_timeline__middle_forwards(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // check first
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    1421183640), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[5].header.id));
  cl_assert(iter_next(&iterator));
  // check second
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[2].header.id));
  // check third
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  // check fourth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));
  // check third again
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  // check done
  cl_assert(iter_next(&iterator));
  cl_assert(iter_next(&iterator) == false);
}

void test_timeline__middle_backwards(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // check first
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    1421183640), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check second
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  // check done
  cl_assert(iter_next(&iterator) == false);
}

static void prv_insert_long_items(void) {
  cl_assert_equal_i(pin_db_flush(), 0);
  for (unsigned int i = 0; i < ARRAY_LENGTH(s_long_items); i++) {
    cl_assert_equal_i(pin_db_insert_item(&s_long_items[i]), 0);
  }
}

void test_timeline__long_middle_past(void) {
  prv_insert_long_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // initialize it to be 11 min after item cc has started
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    14700), 0);
 
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[1].header.id));

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[0].header.id));

  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[1].header.id));

  cl_assert(!iter_prev(&iterator));
}

void test_timeline__long_middle_future(void) {
  prv_insert_long_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  // initialize it to be 11 min after item cc has started
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    14700), 0);

  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[2].header.id));

  cl_assert(iter_next(&iterator));
  
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[3].header.id));

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[4].header.id));
  cl_assert(!iter_next(&iterator));

  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[3].header.id));

  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_long_items[2].header.id));

  cl_assert(!iter_prev(&iterator));
}

static int prv_num_items(Iterator iterator) {
  int n = 1;
  while (iter_next(&iterator)) {
    n++;
  }
  return n;
}

void test_timeline__gc_past(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  // Tue Jan 13 11:40:00 PST 2015
  rtc_set_time(1421178000);
  fake_pbl_malloc_clear_tracking();
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    1421178000), 0);
  cl_assert_equal_i(prv_num_items(iterator), 6);

  // Thursday Jan 16 00:00:00 PST 2015
  // No items within window
  rtc_set_time(1421395200);
  head = NULL;
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
                                       1421395200),
                    S_NO_MORE_ITEMS);

  fake_pbl_malloc_clear_tracking();
  // Thursday Jan 16 14:00:00 PST 2015
  // all items garbage collected
  rtc_set_time(1421445600);
  head = NULL;
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
                                       1421445600),
                    S_NO_MORE_ITEMS);


  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), 0);
}

static void prv_insert_all_day_items(void) {
  for (int i = 0; i < ARRAY_LENGTH(s_all_day_items); i++) {
    cl_assert_equal_i(pin_db_insert_item(&s_all_day_items[i]), 0);
  }
}

void test_timeline__all_day_future(void) {
  prv_insert_all_day_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  // start 11:40 AM, earlier than all timed events for that day
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    1421178000), 0);
  Uuid first_all_day_event = state.pin.header.id;
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[1].header.id) ||
    uuid_equal(&state.pin.header.id, &s_all_day_items[2].header.id));
  cl_assert(state.node->all_day);
  // check that the item we see is timestamped at local midnight rather than utc midnight
  // 1421136000 is midnight Jan 13, PST
  cl_assert_equal_i(state.pin.header.timestamp, 1421136000);

  // second all day event
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[1].header.id) ||
    uuid_equal(&state.pin.header.id, &s_all_day_items[2].header.id));
  cl_assert(!uuid_equal(&first_all_day_event, &state.pin.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.pin.header.timestamp, 1421136000);

  // back to the first
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&first_all_day_event, &state.pin.header.id));
  cl_assert(state.node->all_day);

  // correct end of line behaviour
  cl_assert(!iter_prev(&iterator));
}

void test_timeline__all_day_future_with_others(void) {
  prv_insert_all_day_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  // start 11:40 AM, earlier than all timed events for that day
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    1421178000), 0);
  Uuid first_all_day_event = state.pin.header.id;
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[1].header.id) ||
    uuid_equal(&state.pin.header.id, &s_all_day_items[2].header.id));
  cl_assert(state.node->all_day);
  // check that the item we see is timestamped at local midnight rather than utc midnight
  // 1421136000 is midnight Jan 13, PST
  cl_assert_equal_i(state.pin.header.timestamp, 1421136000);

  // second all day event
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[1].header.id) ||
    uuid_equal(&state.pin.header.id, &s_all_day_items[2].header.id));
  cl_assert(!uuid_equal(&first_all_day_event, &state.pin.header.id));
  cl_assert(state.node->all_day);

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  // check second
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check third
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[5].header.id));
  // check second again
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check fourth
  cl_assert(iter_next(&iterator));
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[2].header.id));
  // check fifth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  // check sixth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));

  // check rollover behaviour
  cl_assert(!iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));

  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
}

void test_timeline__all_day_past(void) {
  prv_insert_all_day_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;
  TimelineItem earlier_item = {
    .header = {
      .id = {0x04},
      .parent_id = {0},
      .timestamp = 1421049600 + 9 * 60 * 60, // 9am on Jan 12, 2015
      .duration = 20,
      .type = TimelineItemTypePin,
      .flags = 0,
      .layout = LayoutIdTest,
    }
  };

  cl_assert_equal_i(pin_db_insert_item(&earlier_item), 0);

  // start 11:40 AM, earlier than all timed events for that day
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    1421178000), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &earlier_item.header.id));
  cl_assert(!state.node->all_day);

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[0].header.id));
  cl_assert(state.node->all_day);
  cl_assert(!iter_next(&iterator));

  cl_assert(iter_prev(&iterator));
  cl_assert(!iter_prev(&iterator));
}

void test_timeline__all_day_middle_past(void) {
  prv_insert_all_day_items();

  // 1421183640 is 13:14 on Jan 13, 2015
  // after first timed event of the day but not all of them
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    1421183640), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  // check all day events
  cl_assert(iter_next(&iterator));
  Uuid first_all_day_event = state.pin.header.id;
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[1].header.id) ||
    uuid_equal(&state.pin.header.id, &s_all_day_items[2].header.id));
  cl_assert(state.node->all_day);

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[1].header.id) ||
    uuid_equal(&state.pin.header.id, &s_all_day_items[2].header.id));
  cl_assert(!uuid_equal(&first_all_day_event, &state.pin.header.id));
  cl_assert(state.node->all_day);

  // yesterday's all day event
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_all_day_items[0].header.id));

  cl_assert(!iter_next(&iterator));
}

static void prv_insert_extra_case_items(void) {
  pin_db_flush();
  for (int i = 0; i < ARRAY_LENGTH(s_extra_case_items); i++) {
    cl_assert_equal_i(pin_db_insert_item(&s_extra_case_items[i]), 0);
  }
}

// 5am, no events passed
void test_timeline__extra_case_forwards(void) {
  prv_insert_extra_case_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    s_feb_5_midnight + 5 * 60 * 60), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[0].header.id));

  // check next
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[1].header.id));

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[2].header.id));

  cl_assert(!iter_next(&iterator));
}

// 5am, no events passed
void test_timeline__extra_case_none_backwards(void) {
  prv_insert_extra_case_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    s_feb_5_midnight + 5 * 60 * 60), 2);
}

// 8:16 am. 8:15 event is in future but not 8:00 event
void test_timeline__extra_case_middle_future(void) {
  prv_insert_extra_case_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    s_feb_5_midnight + 8 * SECONDS_PER_HOUR + 16 * SECONDS_PER_MINUTE), 0);

  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[1].header.id));

  cl_assert(iter_next(&iterator));

  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[2].header.id));

  cl_assert(!iter_next(&iterator));
}

// 8:16 am, 8:00 event has passed but not 8:15 event
void test_timeline__extra_case_middle_past(void) {
  prv_insert_extra_case_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    s_feb_5_midnight + 8 * 60 * 60 + 16 * 60), 0);

  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[0].header.id));

  cl_assert(!iter_next(&iterator));
}

// 11 am, all events passed
void test_timeline__extra_case_backwards(void) {
  prv_insert_extra_case_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionPast,
    s_feb_5_midnight + 11 * 60 * 60), 0);
  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[2].header.id));

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[1].header.id));

  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_extra_case_items[0].header.id));

  cl_assert(!iter_next(&iterator));
}

// 11 am
void test_timeline__extra_case_none_forwards(void) {
  prv_insert_extra_case_items();

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
    s_feb_5_midnight + 11 * 60 * 60), 2);
}

void test_timeline__two_iterators(void) {
  uint8_t init_net_allocs = fake_pbl_malloc_num_net_allocs();
  Iterator iterator1 = {0};
  Iterator iterator2 = {0};
  TimelineIterState state1 = {0};
  TimelineIterState state2 = {0};
  TimelineNode *head = NULL;

  // first iterator should alloc all the memory for all items
  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator1, &state1, &head, TimelineIterDirectionFuture,
    1421178000), 0);
  // should have one alloc for each node in list, + 1 for the current timelineitem
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(),
                    init_net_allocs + ARRAY_LENGTH(s_items) + 1);

  // second iterator should not alloc any more memory
  cl_assert_equal_i(timeline_iter_init(&iterator2, &state2, &head, TimelineIterDirectionFuture,
    1421178000), 0);
  // should have one alloc for each node in list, + 1 for the current timelineitem
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(),
                    init_net_allocs + ARRAY_LENGTH(s_items) + 2);

  // deinit should free all the memory
  timeline_iter_deinit(&iterator1, &state1, &head);
  timeline_iter_deinit(&iterator2, &state2, &head);
  cl_assert_equal_i(fake_pbl_malloc_num_net_allocs(), init_net_allocs);
}

void test_timeline__delete_on_iterator(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture, 1421178000), 0);
  // s_items[0] is the earliest pin, followed by s_items[4]
  cl_assert_equal_i(pin_db_delete((uint8_t *)&s_items[0].header.id, sizeof(Uuid)), 0);

  // next item should be items 4, going back should skip over items[0]
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));

  cl_assert(!iter_prev(&iterator));
  timeline_iter_deinit(&iterator, &state, &head);
}

void test_timeline__skip_deleted_item(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture, 1421178000), 0);

  cl_assert_equal_i(pin_db_delete((uint8_t *)&s_items[4].header.id, sizeof(Uuid)), 0);

  // next item should be items[5], going back should skip over items[4]
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[5].header.id));

  cl_assert(iter_prev(&iterator));
  cl_assert(!iter_prev(&iterator));
  cl_assert(state.node);
  timeline_iter_deinit(&iterator, &state, &head);
}

void test_timeline__delete_last_items(void) {
  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  timeline_init(&head);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture, 1421178000), 0);

  // delete item 2, iterate to the end, check that everything still works
  cl_assert_equal_i(pin_db_delete((uint8_t *)&s_items[2].header.id, sizeof(Uuid)), 0);

  cl_assert(uuid_equal(&state.pin.header.id, &s_items[0].header.id));
  // check second
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check third
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[5].header.id));
  // check second again
  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[4].header.id));
  // check fourth
  cl_assert(iter_next(&iterator));
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  // check fifth
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));
  // check sixth
  cl_assert(!iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[3].header.id));
  cl_assert(uuid_equal(&state.node->id, &s_items[3].header.id));
  cl_assert(state.node);

  cl_assert(iter_prev(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &s_items[1].header.id));
  timeline_iter_deinit(&iterator, &state, &head);
}

void test_timeline__multiday(void) {
  TimelineItem multiday_item = {
    .header = {
        .id = {0x29, 0xac, 0xd8, 0xb5, 0x9, 0xc7, 0x4c, 0x31, 0xbf,
          0x6f, 0x3, 0x64, 0xd0, 0x5b, 0x9b, 0xc2},
        .parent_id = {0},
        .timestamp =  1425312000, // 8:00 AM March 2 2015 PST
        .duration = (16 + (2 * 24) + 13) * MINUTES_PER_HOUR, // lasts until March 5 1pm (4 days total)
        .type = TimelineItemTypePin,
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
  };

  cl_assert(timeline_add(&multiday_item));

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  // 1425272400 is 21:00 March 1 2015 PST
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture, 1425272400),
    S_SUCCESS);
  time_t midnight_march_2_pst = 1425283200;

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &multiday_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &multiday_item.header.id));
  cl_assert(!state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, 1425312000);
  cl_assert_equal_i(state.node->duration, 16 * 60);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst);

  // day 2
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &multiday_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &multiday_item.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, 1425369600);
  cl_assert_equal_i(state.node->duration, MINUTES_PER_DAY);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst + SECONDS_PER_DAY);

  // no more
  cl_assert(!iter_next(&iterator));

  // 4 deletes
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
}

void test_timeline__all_day_single_day(void) {
  const time_t midnight_march_3_utc = 1425340800;
  TimelineItem all_day_item = {
    .header = {
      .id = { 0x29, 0xac, 0xd8, 0xb5, 0x09, 0xc7, 0x4c, 0x31,
              0xbf, 0x6f, 0x03, 0x64, 0xd0, 0x5b, 0x9b, 0xc2 },
      .timestamp = midnight_march_3_utc,
      .duration = MINUTES_PER_DAY,
      .type = TimelineItemTypePin,
      .layout = LayoutIdTest,
      .all_day = 1,
    },
  };

  cl_assert(timeline_add(&all_day_item));

  Iterator iterator = {};
  TimelineIterState state = {};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  const time_t time_21_00_march_1_pst = 1425272400;
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
                                       time_21_00_march_1_pst), S_SUCCESS);
  const time_t midnight_march_3_pst = 1425369600;

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &all_day_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &all_day_item.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_3_pst);
  cl_assert_equal_i(state.node->duration, MINUTES_PER_DAY);
  cl_assert_equal_i(state.current_day, midnight_march_3_pst);

  // no more
  cl_assert(!iter_next(&iterator));

  // 1 delete
  cl_assert(timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
}

void test_timeline__24h_non_all_day_starting_mid_day(void) {
  const time_t midnight_march_3_utc = 1425340800;
  TimelineItem all_day_item = {
    .header = {
      .id = { 0x29, 0xac, 0xd8, 0xb5, 0x09, 0xc7, 0x4c, 0x31,
              0xbf, 0x6f, 0x03, 0x64, 0xd0, 0x5b, 0x9b, 0xc2 },
      .timestamp = midnight_march_3_utc,
      .duration = MINUTES_PER_DAY,
      .type = TimelineItemTypePin,
      .layout = LayoutIdTest,
      .all_day = 0, // this is a non-all-day event spanning 24h
    },
  };

  cl_assert(timeline_add(&all_day_item));

  Iterator iterator = {};
  TimelineIterState state = {};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  const time_t time_21_00_march_1_pst = 1425272400;
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
                                       time_21_00_march_1_pst), S_SUCCESS);
  const time_t midnight_march_2_pst = 1425283200;

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &all_day_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &all_day_item.header.id));
  cl_assert(!state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_3_utc);
  cl_assert_equal_i(state.node->duration, 8 * MINUTES_PER_HOUR);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst);

  // day 2
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &all_day_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &all_day_item.header.id));
  cl_assert(!state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_3_utc + SECONDS_PER_DAY);
  cl_assert_equal_i(state.node->duration, 0);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst + SECONDS_PER_DAY);

  // no more
  cl_assert(!iter_next(&iterator));

  // 2 deletes
  cl_assert(timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
}

void test_timeline__24h_non_all_day_starting_midnight(void) {
  const time_t midnight_march_2_pst = 1425283200;
  TimelineItem all_day_item = {
    .header = {
      .id = { 0x29, 0xac, 0xd8, 0xb5, 0x09, 0xc7, 0x4c, 0x31,
              0xbf, 0x6f, 0x03, 0x64, 0xd0, 0x5b, 0x9b, 0xc2 },
      .timestamp = midnight_march_2_pst,
      .duration = MINUTES_PER_DAY,
      .type = TimelineItemTypePin,
      .layout = LayoutIdTest,
      .all_day = 0, // this is a non-all-day event spanning 24h
    },
  };

  cl_assert(timeline_add(&all_day_item));

  Iterator iterator = {};
  TimelineIterState state = {};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  const time_t time_21_00_march_1_pst = 1425272400;
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
                                       time_21_00_march_1_pst), S_SUCCESS);

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &all_day_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &all_day_item.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_2_pst);
  cl_assert_equal_i(state.node->duration, MINUTES_PER_DAY);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst);

  // no more
  cl_assert(!iter_next(&iterator));

  // 1 delete
  cl_assert(timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
}

void test_timeline__all_day_multiday(void) {
  TimelineItem multiday_item = {
    .header = {
        .id = {0x29, 0xac, 0xd8, 0xb5, 0x9, 0xc7, 0x4c, 0x31, 0xbf,
          0x6f, 0x3, 0x64, 0xd0, 0x5b, 0x9b, 0xc2},
        .parent_id = {0},
        .timestamp =  1425254400, // midnight March 2 2015 UTC
        .duration = 4 * MINUTES_PER_DAY,
        .type = TimelineItemTypePin,
        .layout = LayoutIdTest,
        .all_day = 1,
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
  };

  cl_assert(timeline_add(&multiday_item));

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  // 1425272400 is 21:00 March 1 2015 PST
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture, 1425272400),
    S_SUCCESS);
  time_t midnight_march_2_pst = 1425283200;

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &multiday_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &multiday_item.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_2_pst);
  cl_assert_equal_i(state.node->duration, MINUTES_PER_DAY);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst);

  // day 2
  cl_assert(iter_next(&iterator));
  cl_assert(uuid_equal(&state.pin.header.id, &multiday_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &multiday_item.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_2_pst + SECONDS_PER_DAY);
  cl_assert_equal_i(state.node->duration, MINUTES_PER_DAY);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst + SECONDS_PER_DAY);

  // no more
  cl_assert(!iter_next(&iterator));

  // 4 deletes
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &multiday_item.header.id));
}

void test_timeline__all_day_ios_bug(void) {
  TimelineItem item = {
    .header = {
        .id = {0x29, 0xac, 0xd8, 0xb5, 0x9, 0xc7, 0x4c, 0x31, 0xbf,
          0x6f, 0x3, 0x64, 0xd0, 0x5b, 0x9b, 0xc2},
        .parent_id = {0},
        .timestamp =  1430236800, // 9am Apr 28, 2015 PDT
        .duration = MINUTES_PER_DAY,
        .type = TimelineItemTypePin,
        .layout = LayoutIdTest,
        .all_day = 1,
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
  };

  cl_assert_equal_i(pin_db_flush(), 0);
  cl_assert(timeline_add(&item));

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
      1430236800 - 60 * 60), S_SUCCESS);
  const time_t midnight_apr_28_pst = 1430208000;

  cl_assert_equal_i(state.node->timestamp, midnight_apr_28_pst);
}

void test_timeline__all_day_ios_bug_2(void) {
  TimelineItem item = {
    .header = {
        .id = {0x29, 0xac, 0xd8, 0xb5, 0x9, 0xc7, 0x4c, 0x31, 0xbf,
          0x6f, 0x3, 0x64, 0xd0, 0x5b, 0x9b, 0xc2},
        .parent_id = {0},
        .timestamp =  1430200800, // 9am Apr 28, 2015 MSK
        .duration = MINUTES_PER_DAY,
        .type = TimelineItemTypePin,
        .layout = LayoutIdTest,
        .all_day = 1,
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
  };

  TimezoneInfo moscow_tz = {
    .tm_gmtoff = 3 * 60 * 60, // MSK
  };

  time_util_update_timezone(&moscow_tz);
  cl_assert_equal_i(pin_db_flush(), 0);
  cl_assert(timeline_add(&item));

  Iterator iterator = {0};
  TimelineIterState state = {0};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
      1430200800 - 60 * 60), S_SUCCESS);
  const time_t midnight_apr_28_msk = 1430168400;

  cl_assert_equal_i(state.node->timestamp, midnight_apr_28_msk);
}

void test_timeline__0_duration_all_day(void) {
  const time_t midnight_march_3_utc = 1425340800;
  TimelineItem all_day_item = {
    .header = {
      .id = { 0x29, 0xac, 0xd8, 0xb5, 0x09, 0xc7, 0x4c, 0x31,
              0xbf, 0x6f, 0x03, 0x64, 0xd0, 0x5b, 0x9b, 0xc2 },
      .timestamp = midnight_march_3_utc,
      .duration = 0,
      .type = TimelineItemTypePin,
      .layout = LayoutIdTest,
      .all_day = 1,
    },
  };

  cl_assert(timeline_add(&all_day_item));

  Iterator iterator = {};
  TimelineIterState state = {};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  const time_t time_21_00_march_1_pst = 1425272400;
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
                                       time_21_00_march_1_pst), S_SUCCESS);
  const time_t midnight_march_3_pst = 1425369600;

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &all_day_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &all_day_item.header.id));
  cl_assert(state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_3_pst);
  cl_assert_equal_i(state.node->duration, MINUTES_PER_DAY);
  cl_assert_equal_i(state.current_day, midnight_march_3_pst);

  // no more
  cl_assert(!iter_next(&iterator));

  // 1 delete
  cl_assert(timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
}

void test_timeline__0_duration(void) {
  const time_t midnight_march_3_utc = 1425340800;
  TimelineItem all_day_item = {
    .header = {
      .id = { 0x29, 0xac, 0xd8, 0xb5, 0x09, 0xc7, 0x4c, 0x31,
              0xbf, 0x6f, 0x03, 0x64, 0xd0, 0x5b, 0x9b, 0xc2 },
      .timestamp = midnight_march_3_utc,
      .duration = 0,
      .type = TimelineItemTypePin,
      .layout = LayoutIdTest,
      .all_day = 0,
    },
  };

  cl_assert(timeline_add(&all_day_item));

  Iterator iterator = {};
  TimelineIterState state = {};
  TimelineNode *head = NULL;

  cl_assert_equal_i(timeline_init(&head), S_SUCCESS);
  const time_t time_21_00_march_1_pst = 1425272400;
  cl_assert_equal_i(timeline_iter_init(&iterator, &state, &head, TimelineIterDirectionFuture,
                                       time_21_00_march_1_pst), S_SUCCESS);
  const time_t midnight_march_2_pst = 1425283200;

  // day 1
  cl_assert(uuid_equal(&state.pin.header.id, &all_day_item.header.id));
  cl_assert(uuid_equal(&state.node->id, &all_day_item.header.id));
  cl_assert(!state.node->all_day);
  cl_assert_equal_i(state.node->timestamp, midnight_march_3_utc);
  cl_assert_equal_i(state.node->duration, 0);
  cl_assert_equal_i(state.current_day, midnight_march_2_pst);

  // no more
  cl_assert(!iter_next(&iterator));

  // 1 delete
  cl_assert(timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
  cl_assert(!timeline_iter_remove_node_with_id(&head, &all_day_item.header.id));
}
