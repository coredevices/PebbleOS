/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <time.h>

#include <pbl/fs/fs.h>
#include <pbl/kernel/compiler.h>
#include <pbl/services/event_service.h>
#include <pbl/services/settings/settings_file.h>
#include <pbl/services/wakeup.h>

#include <clar.h>
#include <flash_region/flash_region.h>
#include <process_management/app_install_manager.h>
#include <syscall/syscall.h>

// Fakes
//////////////////////////////////////////////////////////
#include <pbl/util/time.h>

#include <fake_app_manager.h>
#include <fake_new_timer.h>
#include <fake_pbl_malloc.h>
#include <fake_pfs.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fake_system_task.h>
#include <fake_time.h>

#define OTHER_UUID                                                                             \
  UuidMake(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, \
           0x0F, 0x10)

typedef struct PBL_PACKED {
  Uuid uuid;
  int32_t reason;
  bool repeating;
  uint16_t repeat_hours_offset;
  bool notify_if_missed;
  int32_t timestamp;
  bool utc;
} TestWakeupEntry;

static void prv_store_entry(WakeupId wakeup_id, Uuid uuid, time_t timestamp) {
  TestWakeupEntry entry = {.uuid = uuid, .timestamp = timestamp};
  SettingsFile file;
  cl_must_pass(settings_file_open(&file, "/wakeup", 8 * 1024));
  cl_must_pass(settings_file_set(&file, &wakeup_id, sizeof(wakeup_id), &entry, sizeof(entry)));
  settings_file_close(&file);
}

// Stubs
////////////////////////////////////
// int g_pbl_log_level = 0;
// void pbl_log(uint8_t level, const char* src_filename, int src_line_number, const char* fmt, ...)
// {}
bool sys_clock_is_24h_style(void) {
  return false;
}

void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback start_cb,
                        EventServiceRemoveSubscriberCallback stop_cb) {
}

static bool s_popup_occurred;
void wakeup_popup_window(uint8_t missed_apps_count, uint8_t *missed_apps_banks) {
  s_popup_occurred = true;
}

bool clock_is_timezone_set(void) {
  return false;
}

static int s_stationary_wake_up_count;
void stationary_wake_up(void) {
  s_stationary_wake_up_count++;
}

// Tests
///////////////////////////////////////////////////////////
void test_wakeup__initialize(void) {
  // Wednesday (the 1st) at 00:00
  // date -d "2014/01/01 00:00:00" "+%s" ==> 1388563200
  fake_rtc_init(0, 1388563200);

  // Init fake filesystem used to load/store wakeup events
  fake_spi_flash_init(0, 0x1000000); // from test_settings_file.c
  fake_pfs_mount();

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);
  stub_app_init();

  // Reset variable due to previous callbacks
  s_popup_occurred = false;
  s_stationary_wake_up_count = 0;

  wakeup_init();
  wakeup_enable(true);
}

void test_wakeup__cleanup(void) {
}

void test_wakeup__basic_checks(void) {
  WakeupId wakeup_id = 0;
  cl_assert_equal_i(sys_get_time(), 1388563200);

  sys_wakeup_cancel_all_for_app();

  // Schedule a wakeup in 10 seconds
  wakeup_id = sys_wakeup_schedule(sys_get_time() + 10, 0, false);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), sys_get_time() + 10);

  // Cancel wakeup event
  sys_wakeup_delete(wakeup_id);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);

  // Schedule again
  wakeup_id = sys_wakeup_schedule(sys_get_time() + 10, 0, false);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), sys_get_time() + 10);

  // Cancel all wakeup events
  sys_wakeup_cancel_all_for_app();
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);
}

void test_wakeup__max_events(void) {
  WakeupId wakeup_id = 0;
  sys_wakeup_cancel_all_for_app();

  // Schedule 8 (max), at 1 minute offsets, then fail on 9th
  for (int i = 1; i <= MAX_WAKEUP_EVENTS_PER_APP; i++) {
    wakeup_id = sys_wakeup_schedule(sys_get_time() + (i * WAKEUP_EVENT_WINDOW), 0, false);
    cl_assert_equal_i(sys_wakeup_query(wakeup_id), sys_get_time() + (i * WAKEUP_EVENT_WINDOW));
  }

  // Test that the 9th wakeup event fails to schedule (E_DOES_NOT_EXIST)
  wakeup_id = sys_wakeup_schedule(
      sys_get_time() + ((MAX_WAKEUP_EVENTS_PER_APP + 1) * WAKEUP_EVENT_WINDOW), 0, false);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);
}

void test_wakeup__gap(void) {
  WakeupId wakeup_id = 0;
  sys_wakeup_cancel_all_for_app();

  // Schedule 1 event in a minute
  wakeup_id = sys_wakeup_schedule(sys_get_time() + WAKEUP_EVENT_WINDOW, 0, false);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), sys_get_time() + WAKEUP_EVENT_WINDOW);

  // Test that another event < 1 minute away fails to schedule (E_DOES_NOT_EXIST)
  wakeup_id = sys_wakeup_schedule(sys_get_time() + WAKEUP_EVENT_WINDOW + 59, 0, false);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);

  // Test that another event < 1 minute away fails to schedule (E_DOES_NOT_EXIST)
  wakeup_id = sys_wakeup_schedule(sys_get_time() + 1, 0, false);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);
}

// work around system_task_add_callback
extern void wakeup_dispatcher_system_task(void *data);

void test_wakeup__out_of_order_schedule(void) {
  const time_t start_time = sys_get_time();
  sys_wakeup_cancel_all_for_app();

  // Schedule a wakeup for 10 windows into the future
  time_t late_event = start_time + WAKEUP_EVENT_WINDOW * 10;
  WakeupId late_wakeup_id = sys_wakeup_schedule(late_event, 0, false);
  cl_assert_equal_i(sys_wakeup_query(late_wakeup_id), late_event);

  // Schedule a wakeup for 5 windows into the future
  time_t early_event = start_time + WAKEUP_EVENT_WINDOW * 5;
  WakeupId early_wakeup_id = sys_wakeup_schedule(early_event, 0, false);
  cl_assert_equal_i(sys_wakeup_query(early_wakeup_id), early_event);

  cl_assert_equal_i(early_wakeup_id, wakeup_get_next_scheduled());

  // Set time 5 minutes into the future, early_event should fire
  rtc_set_time(early_event);

  // Force wakeup to check for current wakeup event.
  wakeup_enable(false);
  wakeup_enable(true);

  // Simulate the firing of the early event
  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)early_wakeup_id);

  // Make sure early_wakeup_id not scheduled
  cl_assert_equal_i(sys_wakeup_query(early_wakeup_id), E_DOES_NOT_EXIST);
  cl_assert_equal_i(sys_wakeup_query(late_wakeup_id), late_event);

  // Make sure that the next scheduled timer is now the late wakeup id.
  cl_assert_equal_i(late_wakeup_id, wakeup_get_next_scheduled());

  // Set time 10 minutes into the future, late_event should fire
  rtc_set_time(late_event);

  // Force wakeup to check for current wakeup event.
  wakeup_enable(false);
  wakeup_enable(true);

  // Simulate the firing of the late event
  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)late_wakeup_id);

  // There should now be no scheduled wakeups
  cl_assert_equal_i(sys_wakeup_query(late_wakeup_id), E_DOES_NOT_EXIST);
}

void test_wakeup__time_jump(void) {
  sys_wakeup_cancel_all_for_app();

  // Schedule 1 event in a minute
  time_t first_event = sys_get_time() + WAKEUP_EVENT_WINDOW;
  WakeupId first_wakeup_id = sys_wakeup_schedule(first_event, 0, false);
  cl_assert_equal_i(sys_wakeup_query(first_wakeup_id), first_event);

  TimerID first_timer = wakeup_get_current();

  // Schedule another a minute away
  time_t second_event = sys_get_time() + WAKEUP_EVENT_WINDOW * 2;
  WakeupId second_wakeup_id = sys_wakeup_schedule(second_event, 0, false);
  cl_assert_equal_i(sys_wakeup_query(second_wakeup_id), second_event);

  TimerID test_timer = wakeup_get_current();

  // Wakeup should still return the first event as scheduled
  cl_assert_equal_i(first_timer, test_timer);

  // Schedule another in the future
  time_t third_event = sys_get_time() + WAKEUP_EVENT_WINDOW * 3;
  WakeupId third_wakeup_id = sys_wakeup_schedule(third_event, 0, false);
  cl_assert_equal_i(sys_wakeup_query(third_wakeup_id), third_event);

  // Schedule another in the future
  time_t fourth_event = sys_get_time() + WAKEUP_EVENT_WINDOW * 4;
  WakeupId fourth_wakeup_id = sys_wakeup_schedule(fourth_event, 0, false);
  cl_assert_equal_i(sys_wakeup_query(fourth_wakeup_id), fourth_event);

  // Jump to the future right before the 3rd event
  rtc_set_time(sys_get_time() + 170);

  // Force wakeup to check for current wakeup event
  wakeup_enable(false);
  wakeup_enable(true);

  // fire the first wakeup event, as it is still current
  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)first_wakeup_id);

  // The current timer should be the second event, even though it is in the past, and should
  // have a WAKEUP_CATCHUP_WINDOW second gap scheduled
  TimerID gap_timer = wakeup_get_current();
  cl_assert_equal_i(stub_new_timer_timeout(gap_timer) / 1000, WAKEUP_CATCHUP_WINDOW);

  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)second_wakeup_id);

  // The current timer should be the third event, with a WAKEUP_CATCHUP_WINDOW second gap again
  // (catchup)
  gap_timer = wakeup_get_current();
  cl_assert_equal_i(stub_new_timer_timeout(gap_timer) / 1000, WAKEUP_CATCHUP_WINDOW);

  rtc_set_time(third_event); // manually move time forward to after third event
  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)third_wakeup_id);

  // Catchup should be finished, gap should be back to >= WAKEUP_CATCHUP_WINDOW seconds
  gap_timer = wakeup_get_current();

  cl_assert_equal_b((stub_new_timer_timeout(gap_timer) / 1000) > WAKEUP_CATCHUP_WINDOW, true);
}

void test_wakeup__handle_clock_change_not_scheduled(void) {
  // Test clock change without wakeup event scheduled
  wakeup_handle_clock_change();

  // Make sure no wakeup event is scheduled
  cl_assert_equal_i(sys_wakeup_query(wakeup_get_next_scheduled()), E_DOES_NOT_EXIST);
  // There should be no wakeup event missed or popup displayed
  cl_assert_equal_b(s_popup_occurred, false);
}

void test_wakeup__handle_clock_change_scheduled_jump(void) {
  // Schedule event timer 1 minute away with notifying on missed event
  time_t first_event = sys_get_time() + WAKEUP_EVENT_WINDOW;
  WakeupId first_wakeup_id = sys_wakeup_schedule(first_event, 0, true);
  cl_assert_equal_i(sys_wakeup_query(first_wakeup_id), first_event);

  TimerID first_timer = wakeup_get_current();

  // Jump 30 seconds in the future
  uint32_t initial_timeout = stub_new_timer_timeout(first_timer);
  uint32_t time_jump_seconds = 30;
  rtc_set_time(sys_get_time() + time_jump_seconds);

  // Notify clock change and record change in new timer
  wakeup_handle_clock_change();
  uint32_t final_timeout = stub_new_timer_timeout(first_timer);

  // Compare to expected value for new timer
  cl_assert_equal_i(final_timeout, initial_timeout - time_jump_seconds * 1000);
  // There should be no wakeup event missed or popup displayed
  cl_assert_equal_b(s_popup_occurred, false);

  // Jump the remainder plus an offset (missing the event)
  rtc_set_time(sys_get_time() + final_timeout / 1000 + time_jump_seconds);
  // A large jump that misses an event is a significant clock change: that is
  // the path that deletes past events and raises the missed-event popup.
  wakeup_handle_significant_clock_change();

  // There should be a missed wakeup event and a popup displayed
  cl_assert_equal_b(s_popup_occurred, true);
  // Make sure the wakeup event is no longer scheduled
  cl_assert_equal_i(sys_wakeup_query(first_timer), E_DOES_NOT_EXIST);
}

void test_wakeup__id_independent_of_timestamp(void) {
  sys_wakeup_cancel_all_for_app();

  WakeupId far_id = sys_wakeup_schedule(INT32_MAX, 0, false);
  cl_assert(far_id > 0);
  cl_assert(far_id != INT32_MAX);

  // A reboot must not derive the next ID from the far timestamp
  wakeup_init();

  WakeupId next_id = sys_wakeup_schedule(sys_get_time() + WAKEUP_EVENT_WINDOW, 0, false);
  cl_assert(next_id > 0);
  cl_assert(next_id != far_id);
  cl_assert_equal_i(sys_wakeup_query(far_id), INT32_MAX);
  cl_assert_equal_i(sys_wakeup_query(next_id), sys_get_time() + WAKEUP_EVENT_WINDOW);
}

void test_wakeup__id_wraps_and_skips_used(void) {
  sys_wakeup_cancel_all_for_app();

  const time_t other_time = sys_get_time() + 10 * WAKEUP_EVENT_WINDOW;
  prv_store_entry(INT32_MAX, OTHER_UUID, other_time);
  prv_store_entry(1, OTHER_UUID, other_time + WAKEUP_EVENT_WINDOW);
  wakeup_init();

  WakeupId wakeup_id = sys_wakeup_schedule(sys_get_time() + WAKEUP_EVENT_WINDOW, 0, false);
  cl_assert_equal_i(wakeup_id, 2);

  // The other app's entries are untouched and not visible to this app
  stub_app_set_uuid(OTHER_UUID);
  cl_assert_equal_i(sys_wakeup_query(INT32_MAX), other_time);
  cl_assert_equal_i(sys_wakeup_query(1), other_time + WAKEUP_EVENT_WINDOW);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);
  stub_app_set_uuid(TEST_UUID);
}

void test_wakeup__invalid_ids_dropped_at_boot(void) {
  sys_wakeup_cancel_all_for_app();

  prv_store_entry(-5, TEST_UUID, sys_get_time() + WAKEUP_EVENT_WINDOW);
  prv_store_entry(0, TEST_UUID, sys_get_time() + 2 * WAKEUP_EVENT_WINDOW);
  wakeup_init();

  SettingsFile file;
  WakeupId key = -5;
  cl_must_pass(settings_file_open(&file, "/wakeup", 8 * 1024));
  cl_assert(!settings_file_exists(&file, &key, sizeof(key)));
  key = 0;
  cl_assert(!settings_file_exists(&file, &key, sizeof(key)));
  settings_file_close(&file);
}

void test_wakeup__gap_only_within_app(void) {
  const time_t event_time = sys_get_time() + WAKEUP_EVENT_WINDOW;

  // Another app filling its quota around the same time can't deny this app
  stub_app_set_uuid(OTHER_UUID);
  WakeupId other_id = sys_wakeup_schedule(event_time, 0, false);
  cl_assert(other_id > 0);
  for (int i = 1; i < MAX_WAKEUP_EVENTS_PER_APP; i++) {
    cl_assert(sys_wakeup_schedule(event_time + i * WAKEUP_EVENT_WINDOW, 0, false) > 0);
  }

  stub_app_set_uuid(TEST_UUID);
  WakeupId wakeup_id = sys_wakeup_schedule(event_time, 0, false);
  cl_assert(wakeup_id > 0);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), event_time);

  // The window still applies to the app's own events
  cl_assert_equal_i(sys_wakeup_schedule(event_time + WAKEUP_EVENT_WINDOW - 1, 0, false), E_RANGE);
}

void test_wakeup__simultaneous_events_spaced(void) {
  const time_t event_time = sys_get_time() + WAKEUP_EVENT_WINDOW;

  stub_app_set_uuid(OTHER_UUID);
  WakeupId other_id = sys_wakeup_schedule(event_time, 0, false);
  cl_assert(other_id > 0);
  stub_app_set_uuid(TEST_UUID);
  WakeupId wakeup_id = sys_wakeup_schedule(event_time, 0, false);
  cl_assert(wakeup_id > 0);

  rtc_set_time(event_time);
  wakeup_enable(false);
  wakeup_enable(true);

  WakeupId first_id = wakeup_get_next_scheduled();
  WakeupId second_id = (first_id == wakeup_id) ? other_id : wakeup_id;
  cl_assert(first_id == wakeup_id || first_id == other_id);

  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)first_id);

  // The second event is delayed instead of preempting the first one right away
  cl_assert_equal_i(wakeup_get_next_scheduled(), second_id);
  cl_assert_equal_i(stub_new_timer_timeout(wakeup_get_current()) / 1000, WAKEUP_CATCHUP_WINDOW);
}

void test_wakeup__far_future_not_early(void) {
  const time_t start_time = sys_get_time();
  const time_t event_time = start_time + 60 * 24 * 60 * 60;

  WakeupId wakeup_id = sys_wakeup_schedule(event_time, 0, false);
  cl_assert(wakeup_id > 0);
  cl_assert_equal_i(wakeup_get_next_scheduled(), wakeup_id);

  uint32_t timeout_s = stub_new_timer_timeout(wakeup_get_current()) / 1000;
  cl_assert(timeout_s > 0);
  cl_assert(timeout_s < event_time - start_time);

  // The timer expiring before the event is due re-arms it instead of firing
  rtc_set_time(start_time + timeout_s);
  stub_new_timer_fire(wakeup_get_current());
  wakeup_dispatcher_system_task((void *)(uintptr_t)wakeup_id);

  cl_assert_equal_i(sys_wakeup_query(wakeup_id), event_time);
  cl_assert_equal_i(wakeup_get_next_scheduled(), wakeup_id);
  cl_assert(stub_new_timer_timeout(wakeup_get_current()) / 1000 > 0);
}

void test_wakeup__dispatch_leaves_stationary(void) {
  const time_t event_time = sys_get_time() + 31 * 60;
  stub_app_set_install_id(1);

  WakeupId wakeup_id = sys_wakeup_schedule(event_time, 0, false);
  cl_assert(wakeup_id > 0);

  // Not yet due, stays in stationary
  rtc_set_time(event_time - 1);
  wakeup_dispatcher_system_task((void *)(uintptr_t)wakeup_id);
  cl_assert_equal_i(s_stationary_wake_up_count, 0);

  rtc_set_time(event_time);
  wakeup_dispatcher_system_task((void *)(uintptr_t)wakeup_id);
  cl_assert_equal_i(s_stationary_wake_up_count, 1);
  cl_assert_equal_i(sys_wakeup_query(wakeup_id), E_DOES_NOT_EXIST);
}
