/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/timeline/reminders.h"

#include "kernel/events.h"
#include "services/normal/filesystem/pfs.h"

#include "clar.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////

#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_pebble_tasks.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "stubs_layout_layer.h"
static time_t now = 0;
static int num_events_put = 0;


time_t rtc_get_time(void) {
  return now;
}

RtcTicks rtc_get_ticks(void) {
  return 0;
}

typedef void (*CallbackEventCallback)(void *data);

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  callback(data);
}

void event_put(PebbleEvent* event) {
  num_events_put++;
}

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_prompt.h"
#include "stubs_regular_timer.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

extern TimerID get_reminder_timer_id(void);

static TimelineItem item1 = {
  .header = {
    .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4},
    .timestamp = 0,
    .duration = 0,
    .type = TimelineItemTypeReminder,
  }  // don't care about the rest
};

static TimelineItem item2 = {
  .header = {
    .id = {0x55, 0xcb, 0x7c, 0x75, 0x8a, 0x35, 0x44, 0x87,
             0x90, 0xa4, 0x91, 0x3f, 0x1f, 0xa6, 0x76, 0x01},
    .timestamp = 100,
    .duration = 0,
    .type = TimelineItemTypeReminder,
  }
};

static TimelineItem item3 = {
  .header = {
    .id = {0x7c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c,
             0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2, 0x05},
    .timestamp = 300,
    .duration = 0,
    .type = TimelineItemTypeReminder,
  }
};

static TimelineItem item4 = {
  .header = {
    .id = {0x8c, 0x65, 0x2e, 0xb9, 0x26, 0xd6, 0x44, 0x2c,
             0x98, 0x68, 0xa4, 0x36, 0x79, 0x7d, 0xe2, 0x05},
    .timestamp = 1337,
    .duration = 0,
    .type = TimelineItemTypeReminder,
  }
};

// Setup
////////////////////////////////////////////////////////////////

void test_reminders__initialize(void) {
  now = 0;
  num_events_put = 0;

  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  reminder_db_init();

  // add all four explicitly out of order
  cl_assert(S_SUCCESS == reminders_insert(&item4));

  cl_assert(S_SUCCESS == reminders_insert(&item2));

  cl_assert(S_SUCCESS == reminders_insert(&item1));

  cl_assert(S_SUCCESS == reminders_insert(&item3));
}

void test_reminders__cleanup(void) {
  //nada
}

// Tests
////////////////////////////////////////////////////////////////

void test_reminders__timer_test(void) {
  cl_assert(stub_new_timer_timeout(get_reminder_timer_id()) == 0);
  cl_assert(memcmp(&item1.header.id, stub_new_timer_callback_data(get_reminder_timer_id()), sizeof(TimelineItemId)) == 0);
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cl_assert(stub_new_timer_fire(get_reminder_timer_id()));
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_invoke(1);
  cl_assert_equal_i(num_events_put, 1);

  // item 2 is now the top reminder...
  cl_assert_equal_m(&item2.header.id, stub_new_timer_callback_data(get_reminder_timer_id()), sizeof(TimelineItemId));
  cl_assert(stub_new_timer_timeout(get_reminder_timer_id()) == 100 * 1000);
  // ...until we insert item 1 back
  cl_assert(S_SUCCESS == reminders_insert(&item1));
  cl_assert_equal_m(&item1.header.id, stub_new_timer_callback_data(get_reminder_timer_id()), sizeof(TimelineItemId));
  cl_assert(stub_new_timer_timeout(get_reminder_timer_id()) == 0);
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cl_assert(stub_new_timer_fire(get_reminder_timer_id()));
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_invoke(1);
  cl_assert_equal_i(num_events_put, 2);

  cl_assert_equal_m(&item2.header.id, stub_new_timer_callback_data(get_reminder_timer_id()), sizeof(TimelineItemId));
  cl_assert(stub_new_timer_timeout(get_reminder_timer_id()) == 100 * 1000);
  now = 100;
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cl_assert(stub_new_timer_fire(get_reminder_timer_id()));
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_invoke(1);
  cl_assert_equal_i(num_events_put, 3);

  cl_assert_equal_m(&item3.header.id, stub_new_timer_callback_data(get_reminder_timer_id()), sizeof(TimelineItemId));
  cl_assert(stub_new_timer_timeout(get_reminder_timer_id()) == 200 * 1000);
  now += 200;
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cl_assert(stub_new_timer_fire(get_reminder_timer_id()));
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_invoke(1);
  cl_assert_equal_i(num_events_put, 4);

  cl_assert_equal_m(&item4.header.id, stub_new_timer_callback_data(get_reminder_timer_id()), sizeof(TimelineItemId));
  cl_assert(stub_new_timer_timeout(get_reminder_timer_id()) == 1037 * 1000);
  now += 1037;
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cl_assert(stub_new_timer_fire(get_reminder_timer_id()));
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_invoke(1);
  cl_assert_equal_i(num_events_put, 5);

  cl_assert(!new_timer_scheduled(get_reminder_timer_id(), NULL));
}

void test_reminders__first_init_test(void) {
  cl_assert_equal_i(reminders_init(), 0);
  test_reminders__timer_test();
}

static TimelineItem s_stale_reminder = {
  .header = {
    .id = {0x3C, 0xAF, 0x17, 0xD5, 0xBE, 0x15, 0x4B, 0xFD, 0xAE, 0x2A,
      0xAE, 0x44, 0xC0, 0x96, 0xCB, 0x7D},
    .timestamp = 60 * 60,
    .duration = 0,
    .type = TimelineItemTypeReminder,
  }
};

void test_reminders__stale_item_insert(void) {
  now = 3 * 60 * 60; // 3 hours after stale_reminder
  cl_assert_equal_i(reminders_insert(&s_stale_reminder), E_INVALID_OPERATION);
}

void test_reminders__stale_item_init(void) {
  cl_assert_equal_i(reminders_insert(&s_stale_reminder), S_SUCCESS);
  stub_new_timer_stop(get_reminder_timer_id());

  now = 1 * 60 * 60;
  reminders_init();
  cl_assert(new_timer_scheduled(get_reminder_timer_id(), NULL));

  now = 3 * 60 * 60;
  reminders_init();
  cl_assert(!new_timer_scheduled(get_reminder_timer_id(), NULL));
}

static TimezoneInfo s_tz = {
  .tm_gmtoff = -8 * 60 * 60, // PST
};

static TimelineItem s_all_day_reminder = {
  .header = {
    .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x67, 0xb4},
    .timestamp = 1425511800, // 23:30 UTC March 4
    .duration = 0,
    .type = TimelineItemTypeReminder,
    .all_day = true,
  }  // don't care about the rest
};

// should show up before s_all_day_reminder even though its timestamp is after due to tz adjustment
static TimelineItem s_reminder_before_all_day_reminder = {
  .header = {
    .id = {0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e,
             0x8d, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x67, 0xb4},
    .timestamp = 1425531600, // 21:00 PST March 4
    .duration = 0,
    .type = TimelineItemTypeReminder,
    .all_day = false,
  }
};

void test_reminders__all_day(void) {
  time_util_update_timezone(&s_tz);
  cl_assert_equal_i(reminders_insert(&s_all_day_reminder), S_SUCCESS);
  cl_assert_equal_i(reminders_insert(&s_reminder_before_all_day_reminder), S_SUCCESS);

  // set time to 16:00 PST March 4
  now = 1425513600;
  reminders_init();
  cl_assert_equal_i(stub_new_timer_timeout(get_reminder_timer_id()), 5 * 60 * 60 * 1000);
  cl_assert(uuid_equal(&s_reminder_before_all_day_reminder.header.id,
    (Uuid *)stub_new_timer_callback_data(get_reminder_timer_id())));
  // set time to 21:00 PST March 4
  now = 1425531600;
  stub_pebble_tasks_set_current(PebbleTask_NewTimers);
  cl_assert(stub_new_timer_fire(get_reminder_timer_id()));
  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  fake_system_task_callbacks_invoke(1);

  cl_assert_equal_i(stub_new_timer_timeout(get_reminder_timer_id()), (2 * 60 + 30) * 60 * 1000);
  cl_assert(uuid_equal(&s_all_day_reminder.header.id,
    (Uuid *)stub_new_timer_callback_data(get_reminder_timer_id())));
}

void test_reminders__stale_all_day(void) {
  time_util_update_timezone(&s_tz);
  // set time to 21:00 PST March 5, when s_all_day_reminder should be rejected for being stale
  now = 1425618000;
  cl_assert_equal_i(reminders_insert(&s_all_day_reminder), E_INVALID_OPERATION);

  // set time to 21:00 PST March 4
  now = 1425531600;
  // if the timestamp of s_all_day_reminder isn't adjusted, it would be rejected for being stale
  // since it "seems" to be timestamped at 15:30 PST, but it should be accepted
  cl_assert_equal_i(reminders_insert(&s_all_day_reminder), S_SUCCESS);
}

void test_reminders__calculate_snooze_time(void) {
  // Test half-time snooze
  now = 0;
  cl_assert_equal_i(50, reminders_calculate_snooze_time(&item2));
  now = 50;
  cl_assert_equal_i(25, reminders_calculate_snooze_time(&item2));

  // Test constant snooze
  now = 80;
  cl_assert_equal_i(600, reminders_calculate_snooze_time(&item2));
  now = 100 + 48 * 60 * 60;
  cl_assert_equal_i(600, reminders_calculate_snooze_time(&item2));

  // Test no snooze
  now = 100 + 48 * 60 * 60 + 1;
  cl_assert_equal_i(0, reminders_calculate_snooze_time(&item2));
}
