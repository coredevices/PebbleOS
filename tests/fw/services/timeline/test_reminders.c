/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/timeline/reminders.h"

#include "kernel/events.h"
#include "pbl/services/blob_db/reminder_db.h"
#include "pbl/services/filesystem/pfs.h"
#include "pbl/services/regular_timer.h"

#include "clar.h"

// Fixture
////////////////////////////////////////////////////////////////

// Fakes
////////////////////////////////////////////////////////////////

#include "fake_pbl_malloc.h"
#include "fake_pebble_tasks.h"
#include "fake_regular_timer.h"
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
#include "stubs_blob_db_sync.h"
#include "stubs_blob_db_sync_util.h"
#include "stubs_crc.h"
#include "stubs_hexdump.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pin_db.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_task_watchdog.h"

extern RegularTimerInfo *get_reminder_timer(void);
extern bool get_reminder_armed(void);
extern time_t get_reminder_timestamp(void);
extern ReminderId *get_reminder_id(void);

// Drives the seconds-callback the same way the regular_timer service would.
// system_task_add_callback is faked to enqueue, so we then drain the queue.
static void prv_advance_to_and_fire(time_t target) {
  now = target;
  fake_regular_timer_trigger(get_reminder_timer());
  fake_system_task_callbacks_invoke(1);
}

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

  // Register the seconds callback so fake_regular_timer_trigger() can drive it.
  cl_assert_equal_i(reminders_init(), S_SUCCESS);

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
  // item1 (timestamp 0) is next; we're at now=0 so it should fire immediately.
  cl_assert(get_reminder_armed());
  cl_assert_equal_i(get_reminder_timestamp(), 0);
  cl_assert_equal_m(&item1.header.id, get_reminder_id(), sizeof(TimelineItemId));
  prv_advance_to_and_fire(0);
  cl_assert_equal_i(num_events_put, 1);

  // item2 (timestamp 100) is now next.
  cl_assert(get_reminder_armed());
  cl_assert_equal_m(&item2.header.id, get_reminder_id(), sizeof(TimelineItemId));
  cl_assert_equal_i(get_reminder_timestamp(), 100);

  // ...until we insert item 1 back; it has timestamp 0 (in the past).
  cl_assert(S_SUCCESS == reminders_insert(&item1));
  cl_assert(get_reminder_armed());
  cl_assert_equal_m(&item1.header.id, get_reminder_id(), sizeof(TimelineItemId));
  cl_assert_equal_i(get_reminder_timestamp(), 0);
  prv_advance_to_and_fire(0);
  cl_assert_equal_i(num_events_put, 2);

  cl_assert_equal_m(&item2.header.id, get_reminder_id(), sizeof(TimelineItemId));
  cl_assert_equal_i(get_reminder_timestamp(), 100);
  prv_advance_to_and_fire(100);
  cl_assert_equal_i(num_events_put, 3);

  cl_assert_equal_m(&item3.header.id, get_reminder_id(), sizeof(TimelineItemId));
  cl_assert_equal_i(get_reminder_timestamp(), 300);
  prv_advance_to_and_fire(300);
  cl_assert_equal_i(num_events_put, 4);

  cl_assert_equal_m(&item4.header.id, get_reminder_id(), sizeof(TimelineItemId));
  cl_assert_equal_i(get_reminder_timestamp(), 1337);
  prv_advance_to_and_fire(1337);
  cl_assert_equal_i(num_events_put, 5);

  // No more reminders pending.
  cl_assert(!get_reminder_armed());
}

void test_reminders__first_init_test(void) {
  cl_assert_equal_i(reminders_init(), 0);
  test_reminders__timer_test();
}

// Verifies the early-fire case: callback runs when RTC has not yet reached the
// reminder timestamp -> nothing should be enqueued.
void test_reminders__not_ready_yet(void) {
  cl_assert_equal_i(reminders_init(), 0);
  // item1 (ts=0) fires immediately. Drain it.
  prv_advance_to_and_fire(0);
  cl_assert_equal_i(num_events_put, 1);

  // item2 has ts=100; before that, tick should do nothing.
  now = 50;
  fake_regular_timer_trigger(get_reminder_timer());
  fake_system_task_callbacks_invoke(1);
  cl_assert_equal_i(num_events_put, 1);
  cl_assert(get_reminder_armed());
  cl_assert_equal_i(get_reminder_timestamp(), 100);

  prv_advance_to_and_fire(100);
  cl_assert_equal_i(num_events_put, 2);
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

  now = 1 * 60 * 60;
  reminders_init();
  cl_assert(get_reminder_armed());

  now = 3 * 60 * 60;
  reminders_init();
  cl_assert(!get_reminder_armed());
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
  cl_assert(get_reminder_armed());
  cl_assert_equal_i(get_reminder_timestamp(), 1425531600);
  cl_assert(uuid_equal(&s_reminder_before_all_day_reminder.header.id,
    (Uuid *)get_reminder_id()));
  // set time to 21:00 PST March 4
  prv_advance_to_and_fire(1425531600);

  // s_all_day_reminder is next.
  cl_assert(get_reminder_armed());
  cl_assert(uuid_equal(&s_all_day_reminder.header.id,
    (Uuid *)get_reminder_id()));
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
