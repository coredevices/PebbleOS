/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "kernel/events.h"
#include "services/normal/blob_db/pin_db.h"
#include "services/normal/timeline/calendar.h"
#include "services/normal/timeline/event.h"
#include "services/normal/timeline/timeline.h"
#include "system/logging.h"

#include "clar.h"

// Stubs
////////////////////////////////////////////////////////////////
#include "stubs_analytics.h"
#include "stubs_ancs.h"
#include "stubs_ancs_notifications.h"
#include "stubs_app_cache.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_manager.h"
#include "stubs_blob_db.h"
#include "stubs_blob_db_sync.h"
#include "stubs_blob_db_sync_util.h"
#include "stubs_event_loop.h"
#include "stubs_event_service_client.h"
#include "stubs_hexdump.h"
#include "stubs_i18n.h"
#include "stubs_layout_layer.h"
#include "stubs_logging.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_notification_storage.h"
#include "stubs_notifications.h"
#include "stubs_passert.h"
#include "stubs_phone_call_util.h"
#include "stubs_prompt.h"
#include "stubs_rand_ptr.h"
#include "stubs_regular_timer.h"
#include "stubs_reminder_db.h"
#include "stubs_session.h"
#include "stubs_sleep.h"
#include "stubs_system_task.h"
#include "stubs_task_watchdog.h"
#include "stubs_text_layer_flow.h"
#include "stubs_timeline.h"
#include "stubs_timeline_pin_window.h"
#include "stubs_window_stack.h"

// Fakes
////////////////////////////////////////////////////////////////
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_pebble_tasks.h"
#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fake_settings_file.h"
#include "fake_events.h"

bool calendar_layout_verify(bool existing_attributes[]) {
  return true;
}

bool weather_layout_verify(bool existing_attributes[]) {
  return true;
}

const TimelineEventImpl *timeline_peek_get_event_service(void) {
  return NULL;
}

// Helpers
////////////////////////////////////////////////////////////////
static bool s_in_calendar_event = false;

static bool prv_get_calendar_ongoing(void) {
  PebbleEvent event = fake_event_get_last();
  if (event.type == PEBBLE_CALENDAR_EVENT) {
    s_in_calendar_event = event.calendar.is_event_ongoing;
  }
  return s_in_calendar_event;
}

// Fake pins
////////////////////////////////////////////////////////////////

static Attribute title_attr = {
  .id = AttributeIdTitle,
  .cstring = "title",
};

static TimelineItem item1 = {
  .header = {
    .id = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .timestamp = 10*60,
    .duration = 10,
    .type = TimelineItemTypePin,
    .all_day = false,
    .layout = LayoutIdCalendar,
  },
  .attr_list = {
    .num_attributes = 1,
    .attributes = &title_attr,
  },
};

static TimelineItem item2 = {
  .header = {
    .id = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .timestamp = 15*60,
    .duration = 20,
    .type = TimelineItemTypePin,
    .all_day = false,
    .layout = LayoutIdCalendar,
  },
  .attr_list = {
    .num_attributes = 1,
    .attributes = &title_attr,
  },
};

static TimelineItem item3 = {
  .header = {
    .id = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .timestamp = 25*60,
    .duration = 5,
    .type = TimelineItemTypePin,
    .all_day = false,
    .layout = LayoutIdCalendar,
  },
  .attr_list = {
    .num_attributes = 1,
    .attributes = &title_attr,
  },
};

static TimelineItem item4 = {
  .header = {
    .id = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .timestamp = 100*60,
    .duration = 10,
    .type = TimelineItemTypePin,
    .all_day = false,
    .layout = LayoutIdCalendar,
  },
  .attr_list = {
    .num_attributes = 1,
    .attributes = &title_attr,
  },
};

// NOT A CALENDAR PIN
static TimelineItem item5 = {
  .header = {
    .id = {0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .timestamp = 10*60,
    .duration = 10,
    .type = TimelineItemTypePin,
    .all_day = false,
    .layout = LayoutIdWeather,
  },
  .attr_list = {
    .num_attributes = 1,
    .attributes = &title_attr,
  },
};

// ALL DAY PIN
static TimelineItem item6 = {
  .header = {
    .id = {0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .timestamp = 100*60,
    .duration = 10,
    .type = TimelineItemTypePin,
    .all_day = true,
    .layout = LayoutIdCalendar,
  },
  .attr_list = {
    .num_attributes = 1,
    .attributes = &title_attr,
  },
};

// Setup
////////////////////////////////////////////////////////////////

void test_calendar__initialize(void) {
  s_in_calendar_event = false;
  rtc_set_time(0);
  fake_event_init();
  pin_db_init();
}

void test_calendar__cleanup(void) {
  timeline_event_deinit();
  stub_new_timer_cleanup();
  fake_settings_file_reset();
}

// Tests
////////////////////////////////////////////////////////////////

void test_calendar__no_events(void) {
  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert_equal_i(timer_id, TIMER_INVALID_ID);
}

void test_calendar__init_with_future_event(void) {
  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);

  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 2);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert(timer_id != TIMER_INVALID_ID);
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(10*60, stub_new_timer_timeout(timer_id) / 1000);
}

void test_calendar_handle__future_event_added_and_removed(void) {
  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert_equal_i(timer_id, TIMER_INVALID_ID);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));

  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 2);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(10*60, stub_new_timer_timeout(timer_id) / 1000);

  cl_assert(timeline_remove(&item1.header.id));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 3);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}

void test_calendar__init_with_ongoing_event(void) {
  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  rtc_set_time(15 * 60);

  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert(timer_id != TIMER_INVALID_ID);
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(5*60, stub_new_timer_timeout(timer_id) / 1000);
}

void test_calendar_handle__ongoing_event_added_and_removed(void) {
  rtc_set_time(15 * 60);
  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert_equal_i(timer_id, TIMER_INVALID_ID);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));

  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 2);
  cl_assert(prv_get_calendar_ongoing());
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(5*60, stub_new_timer_timeout(timer_id) / 1000);

  cl_assert(timeline_remove(&item1.header.id));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 3);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}

void test_calendar__init_with_past_event(void) {
  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  rtc_set_time(30 * 60);

  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert_equal_i(timer_id, TIMER_INVALID_ID);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}

void test_calendar_handle__past_event_added_and_removed(void) {
  rtc_set_time(30 * 60);
  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert_equal_i(timer_id, TIMER_INVALID_ID);
  cl_assert(!stub_new_timer_is_scheduled(timer_id));

  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 2);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(!stub_new_timer_is_scheduled(timer_id));

  cl_assert(timeline_remove(&item1.header.id));
  timeline_event_handle_blobdb_event();
  cl_assert_equal_i(fake_event_get_count(), 3);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}

void test_calendar__timer_test(void) {
  cl_assert(timeline_add(&item1));
  timeline_event_handle_blobdb_event();
  cl_assert(timeline_add(&item2));
  timeline_event_handle_blobdb_event();
  cl_assert(timeline_add(&item3));
  timeline_event_handle_blobdb_event();
  cl_assert(timeline_add(&item4));
  timeline_event_handle_blobdb_event();

  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert(timer_id != TIMER_INVALID_ID);
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(10*60, stub_new_timer_timeout(timer_id) / 1000);

  rtc_set_time(10 * 60);
  cl_assert(stub_new_timer_fire(timer_id));
  cl_assert_equal_i(fake_event_get_count(), 2);
  cl_assert(prv_get_calendar_ongoing());
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(10*60, stub_new_timer_timeout(timer_id) / 1000);

  rtc_set_time(20 * 60);
  cl_assert(stub_new_timer_fire(timer_id));
  cl_assert_equal_i(fake_event_get_count(), 3);
  cl_assert(prv_get_calendar_ongoing());
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(15*60, stub_new_timer_timeout(timer_id) / 1000);

  rtc_set_time(35 * 60);
  cl_assert(stub_new_timer_fire(timer_id));
  cl_assert_equal_i(fake_event_get_count(), 4);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(65*60, stub_new_timer_timeout(timer_id) / 1000);

  rtc_set_time(100 * 60);
  cl_assert(stub_new_timer_fire(timer_id));
  cl_assert_equal_i(fake_event_get_count(), 5);
  cl_assert(prv_get_calendar_ongoing());
  cl_assert(stub_new_timer_is_scheduled(timer_id));
  cl_assert_equal_i(10*60, stub_new_timer_timeout(timer_id) / 1000);

  rtc_set_time(110 * 60);
  cl_assert(stub_new_timer_fire(timer_id));
  cl_assert_equal_i(fake_event_get_count(), 6);
  cl_assert(!prv_get_calendar_ongoing());
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}

void test_calendar__handle_non_calendar_pins(void) {
  // Insert a random pin (non calendar event)
  cl_assert(timeline_add(&item5));
  timeline_event_handle_blobdb_event();

  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert_equal_i(timer_id, TIMER_INVALID_ID);
}

void test_calendar__handle_all_day_pins(void) {
  // Insert an all day pin
  cl_assert(timeline_add(&item6));
  timeline_event_handle_blobdb_event();

  timeline_event_init();
  cl_assert_equal_i(fake_event_get_count(), 1);
  cl_assert(!prv_get_calendar_ongoing());
  TimerID timer_id = stub_new_timer_get_next();
  cl_assert(!stub_new_timer_is_scheduled(timer_id));
}
