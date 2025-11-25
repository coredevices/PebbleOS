/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <time.h>

#include "services/normal/wakeup.h"
#include "syscall/syscall.h"
#include "flash_region/flash_region.h"
#include "services/normal/filesystem/pfs.h"
#include "services/normal/settings/settings_file.h"
#include "services/common/event_service.h"
#include "process_management/app_install_manager.h"

#include "clar.h"

// Fakes
//////////////////////////////////////////////////////////
#include "fake_app_manager.h"
#include "fake_rtc.h"
#include "fake_new_timer.h"
#include "fake_pbl_malloc.h"
#include "fake_spi_flash.h"
#include "fake_system_task.h"
#include "fake_time.h"

#include "stubs_analytics.h"
#include "stubs_events.h"
#include "stubs_language_ui.h"
#include "stubs_logging.h"
#include "stubs_print.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_passert.h"
#include "stubs_pebble_process_md.h"
#include "stubs_rand_ptr.h"
#include "stubs_sleep.h"
#include "stubs_mutex.h"
#include "stubs_hexdump.h"
#include "stubs_task_watchdog.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_memory_layout.h"

#define TEST_UUID UuidMake(0xF9, 0xC6, 0xEB, 0xE4, 0x06, 0xCD, 0x46, 0xF1, 0xB1, 0x51, 0x24, 0x08, 0x74, 0xD2, 0x07, 0x73)


// Stubs
////////////////////////////////////
//int g_pbl_log_level = 0;
//void pbl_log(uint8_t level, const char* src_filename, int src_line_number, const char* fmt, ...) {}
int time_util_get_num_hours(int hours, bool is24h) {return 0;}

bool sys_clock_is_24h_style(void) {return false;}

void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback start_cb,
                        EventServiceRemoveSubscriberCallback stop_cb) {}

static bool s_popup_occurred;
void wakeup_popup_window(uint8_t missed_apps_count, uint8_t *missed_apps_banks) {
  s_popup_occurred = true;
}

static PebbleProcessMd s_test_app_md = { .uuid = TEST_UUID };

bool clock_is_timezone_set(void) {
  return false;
}


// Tests
///////////////////////////////////////////////////////////
void test_wakeup__initialize(void) {
  // Wednesday (the 1st) at 00:00
  // date -d "2014/01/01 00:00:00" "+%s" ==> 1388563200
  fake_rtc_init(0, 1388563200);

  // Init fake filesystem used to load/store wakeup events
  fake_spi_flash_init(0, 0x1000000); //from test_settings_file.c
  pfs_init(false);

  stub_pebble_tasks_set_current(PebbleTask_KernelBackground);

  // Reset variable due to previous callbacks
  s_popup_occurred = false;

  wakeup_init();
  wakeup_enable(true);
}

void test_wakeup__cleanup(void) {}

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
  wakeup_id = sys_wakeup_schedule(sys_get_time() + ((MAX_WAKEUP_EVENTS_PER_APP + 1) * WAKEUP_EVENT_WINDOW), 0, false);
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

  // The current timer should be the third event, with a WAKEUP_CATCHUP_WINDOW second gap again (catchup)
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
  wakeup_handle_clock_change();

  // There should be a missed wakeup event and a popup displayed
  cl_assert_equal_b(s_popup_occurred, true);
  // Make sure the wakeup event is no longer scheduled
  cl_assert_equal_i(sys_wakeup_query(first_timer), E_DOES_NOT_EXIST);
}
