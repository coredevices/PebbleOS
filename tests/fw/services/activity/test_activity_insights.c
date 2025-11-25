/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/normal/activity/activity.h"
#include "services/normal/activity/activity_insights.h"
#include "services/normal/activity/activity_private.h"
#include "services/normal/activity/insights_settings.h"
#include "services/normal/filesystem/pfs.h"
#include "util/attributes.h"

#include <stdint.h>

#include "clar.h"

// Stubs
#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_attribute.h"
#include "stubs_event_service_client.h"
#include "stubs_health_db.h"
#include "stubs_health_util.h"
#include "stubs_i18n.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_rand_ptr.h"
#include "stubs_stringlist.h"
#include "stubs_system_task.h"

// Fakes
#include "fake_kernel_services_notifications.h"
#include "fake_pbl_malloc.h"
#include "fake_rtc.h"
#include "fake_settings_file.h"

// We start time out at 5pm on Jan 1, 2015 for all of these tests
static struct tm s_init_time_tm = {
  // Thursday, Jan 1, 2015, 10:00am
  .tm_hour = 10,
  .tm_mday = 1,
  .tm_mon = 0,
  .tm_year = 115
};

#define ACTIVE_MINUTES 2
#define AVERAGE_STEPS 1000
#define HIGH_STEPS 2000

#define MAX_ACTIVITY_SESSIONS 24

extern void prv_calculate_metric_history_stats(ActivityMetric metric,
                                               ActivityInsightMetricHistoryStats *stats);

// =========================================================================================
// Activity stubs / fakes
typedef struct StaticData {
  ActivityScalarStore steps_per_minute;
  int32_t metric_history[ActivityMetricNumMetrics][ACTIVITY_HISTORY_DAYS];

  // Enough room for 3 days worth of sessions
  ActivitySession activity_sessions[3 * MAX_ACTIVITY_SESSIONS];
  uint32_t num_sessions;

  int pins_added;
  int pins_removed;
  int notifs_shown;
} StaticData;
static StaticData s_data = {};

bool activity_prefs_activity_insights_are_enabled(void) {
  return true;
}

bool activity_prefs_sleep_insights_are_enabled(void) {
  return true;
}

bool activity_get_metric(ActivityMetric metric, uint32_t history_len, int32_t *history) {
  memcpy(history, &s_data.metric_history[metric], history_len * sizeof(int32_t));
  return true;
}


// Update the sleep metrics based on the current set of sleep sessions for today
static void prv_update_sleep_metrics(void) {
  ActivitySession activity_sessions[MAX_ACTIVITY_SESSIONS];
  uint32_t num_sessions = MAX_ACTIVITY_SESSIONS;

  activity_get_sessions(&num_sessions, activity_sessions);
  uint32_t total_seconds = 0;
  time_t sleep_enter_utc = -1;
  time_t sleep_exit_utc = -1;
  for (uint32_t i = 0; i < num_sessions; i++) {
    time_t exit_utc = activity_sessions[i].start_utc
                    + activity_sessions[i].length_min * SECONDS_PER_MINUTE;
    if (activity_sessions[i].type == ActivitySessionType_Sleep) {
      if (sleep_enter_utc == -1) {
        sleep_enter_utc = activity_sessions[i].start_utc;
      }
      if (sleep_exit_utc == -1 || exit_utc > sleep_exit_utc) {
        sleep_exit_utc = exit_utc;
      }
    }
    total_seconds += activity_sessions[i].length_min * SECONDS_PER_MINUTE;
  }

  s_data.metric_history[ActivityMetricSleepEnterAtSeconds][0] =
                 time_util_get_minute_of_day(sleep_enter_utc) * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepExitAtSeconds][0] =
                 time_util_get_minute_of_day(sleep_exit_utc) * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepTotalSeconds][0] = total_seconds;
}


void activity_sessions_prv_get_sleep_bounds_utc(time_t now_utc, time_t *enter_utc, time_t *exit_utc) {
  ActivitySession activity_sessions[MAX_ACTIVITY_SESSIONS];
  uint32_t num_sessions = MAX_ACTIVITY_SESSIONS;

  activity_get_sessions(&num_sessions, activity_sessions);
  uint32_t total_seconds = 0;
  *enter_utc = 0;
  *exit_utc = 0;
  for (uint32_t i = 0; i < num_sessions; i++) {
    if (activity_sessions[i].type != ActivitySessionType_Sleep) {
      continue;
    }
    if (*enter_utc == 0) {
      *enter_utc = activity_sessions[i].start_utc;
    }
    time_t session_exit_utc = activity_sessions[i].start_utc
                              + activity_sessions[i].length_min * SECONDS_PER_MINUTE;
    if (*exit_utc == 0 || session_exit_utc > *exit_utc) {
      *exit_utc = session_exit_utc;
    }
  }
}


// Appends a new sleep session to the sleep sessions array and increments the current SleepExit and
// SleepTotal metrics accordingly
void prv_add_sleep_or_nap_session(ActivitySessionType session_type, double offset_hours,
                                  double length_hours) {
  int32_t offset_sec = offset_hours * SECONDS_PER_HOUR;
  uint32_t length_min = length_hours * MINUTES_PER_HOUR;
  uint32_t length_sec = length_hours * SECONDS_PER_HOUR;

  int prev = s_data.num_sessions - 1;

  time_t midnight = time_util_get_midnight_of(rtc_get_time());
  time_t previous_exit_utc;
  if (s_data.num_sessions > 0) {
    previous_exit_utc = s_data.activity_sessions[prev].start_utc
                        + (s_data.activity_sessions[prev].length_min * SECONDS_PER_MINUTE);
  } else {
    previous_exit_utc = midnight;
  }

  time_t start_utc = previous_exit_utc + offset_sec;
  if (start_utc > rtc_get_time()) {
    printf("now_utc: %ld, start_utc: %ld\n", rtc_get_time(), start_utc);
  }
  cl_assert(start_utc <= rtc_get_time());
  if (start_utc + length_sec > rtc_get_time()) {
    printf("now_utc: %ld, end_utc: %ld\n", rtc_get_time(), start_utc + length_sec);
  }
  cl_assert(start_utc + length_sec <= rtc_get_time());
  s_data.activity_sessions[s_data.num_sessions++] = (ActivitySession) {
    .type = session_type,
    .length_min = length_min,
    .start_utc = start_utc,
  };

  // Update 'current' metrics
  prv_update_sleep_metrics();
}

// Appends a new sleep session to the sleep sessions array and increments the current SleepExit and
// SleepTotal metrics accordingly
void prv_add_sleep_session(double offset_hours, double length_hours) {
  prv_add_sleep_or_nap_session(ActivitySessionType_Sleep, offset_hours, length_hours);
}

// Appends a new nap session to the sleep sessions array and increments the current SleepExit and
// SleepTotal metrics accordingly
void prv_add_nap_session(double offset_hours, double length_hours) {
  prv_add_sleep_or_nap_session(ActivitySessionType_Nap, offset_hours, length_hours);
}

void prv_add_walk_session(double offset_hours, double length_hours) {
  const uint32_t length_min = length_hours * MINUTES_PER_HOUR;

  s_data.activity_sessions[s_data.num_sessions++] = (ActivitySession) {
    .type = ActivitySessionType_Walk,
    .length_min = length_min,
    .start_utc = rtc_get_time(),
    .step_data = {
      .steps = length_min * 60,
      .active_kcalories = length_min * 2,
      .resting_kcalories = length_min / 10,
      .distance_meters = (length_min * 1000) / 30,
    },
  };
}

bool activity_get_sessions(uint32_t *session_entries, ActivitySession *sessions) {
  // Only return the sleep sessions that belong to "today"
  time_t start_of_today_utc = time_util_get_midnight_of(rtc_get_time());
  int last_sleep_second_of_day = ACTIVITY_LAST_SLEEP_MINUTE_OF_DAY * SECONDS_PER_MINUTE;
  time_t sleep_earliest_end_utc = start_of_today_utc
                                - (SECONDS_PER_DAY - last_sleep_second_of_day);

  uint32_t num_sessions_returned = 0;
  for (uint32_t i = 0; i < s_data.num_sessions; i++) {
    if (num_sessions_returned >= *session_entries) {
      // No more room
      break;
    }
    time_t session_end = s_data.activity_sessions[i].start_utc
                       + s_data.activity_sessions[i].length_min * SECONDS_PER_MINUTE;
    if (session_end >= sleep_earliest_end_utc) {
      // This session should be included in today's sessions
      sessions[num_sessions_returned++] = s_data.activity_sessions[i];
    }
  }
  *session_entries = num_sessions_returned;
  return true;
}

SettingsFile *activity_private_settings_open(void) {
  static SettingsFile file = {};
  return &file;
}

void activity_private_settings_close(SettingsFile *file) {
  return;
}

bool activity_get_step_averages(DayInWeek day_of_week, ActivityMetricAverages *averages) {
  return false;
}

static time_t s_activation_time = 0;
time_t activity_prefs_get_activation_time(void) {
  return s_activation_time;
}

static void prv_set_activation_time(time_t activation_time) {
  s_activation_time = activation_time;
}

static uint32_t s_activity_activation_delay_insight_bitmask = 0;
bool activity_prefs_has_activation_delay_insight_fired(ActivationDelayInsightType type) {
  return (s_activity_activation_delay_insight_bitmask & (1 << type));
}

void activity_prefs_set_activation_delay_insight_fired(ActivationDelayInsightType type) {
  s_activity_activation_delay_insight_bitmask |= (1 << type);
}

static int s_health_app_opened_version = 0;

uint8_t activity_prefs_get_health_app_opened_version(void) {
  return s_health_app_opened_version;
}

ActivityScalarStore activity_metrics_prv_steps_per_minute(void) {
  return s_data.steps_per_minute;
}


// =========================================================================================
// PFS stubs
static PFSFileChangedCallback pfs_watch_cb = NULL;
PFSCallbackHandle pfs_watch_file(const char* filename, PFSFileChangedCallback callback,
                                 uint8_t event_flags, void* data) {
  pfs_watch_cb = callback;
  return NULL;
}

void pfs_unwatch_file(PFSCallbackHandle cb_handle) {
  pfs_watch_cb = NULL;
}

// =========================================================================================
// Timeline item stubs
static TimelineItem s_item = {};
TimelineItem *timeline_item_create_with_attributes(time_t timestamp, uint16_t duration,
                                                   TimelineItemType type, LayoutId layout,
                                                   AttributeList *attr_list,
                                                   TimelineItemActionGroup *action_group) {
  uuid_generate(&s_item.header.id);
  return &s_item;
}

void timeline_item_destroy(TimelineItem* item) {
  return;
}

// =========================================================================================
// Timeline stubs
static Uuid s_last_timeline_id;
bool timeline_add(TimelineItem *item) {
  s_last_timeline_id = item->header.id;
  s_data.pins_added++;
  return true;
}

bool timeline_remove(Uuid *id) {
  s_data.pins_removed++;
  return true;
}

bool timeline_exists(Uuid *id) {
  return true;
}

// =========================================================================================
// Notification stubs
void notification_storage_store(TimelineItem* notification) {
  s_data.notifs_shown++;
}

// Helpers
static void prv_set_time(const struct tm *input) {
  struct tm time_tm = *input;
  time_t utc_sec = mktime(&time_tm);
  rtc_set_time(utc_sec);

  s_activity_activation_delay_insight_bitmask = 0;
  s_activation_time = 0;
  s_activity_activation_delay_insight_bitmask = 0;
}

// =============================================================================================
// Start of unit tests
void test_activity_insights__initialize(void) {
  prv_set_time(&s_init_time_tm);

  fake_kernel_services_notifications_reset();
  s_activation_time = 0;
  s_health_app_opened_version = 0;

  s_data = (StaticData) {};
}

// ---------------------------------------------------------------------------------------
void test_activity_insights__cleanup(void) {
  fake_settings_file_reset();
}

// ---------------------------------------------------------------------------------------
// Test that we correctly calculate the statistics (# days of history, median, etc)
void test_activity_insights__calculate_metric_history_stats(void) {
  // Construct history
  static const int32_t complete_history[ACTIVITY_HISTORY_DAYS] = {
    1234, // This value is ignored since it's loaded in as the current value
    6233, 4277, 9857, 4737, 6540, 719, 9917, 7019, 6347, 4704, 5050, 8370, 4200, 8284, 6664,
    9177, 9734, 2330, 3951, 1568, 871, 776, 8751, 987, 7813, 772, 5079, 7438, 428
  };
  memcpy(&s_data.metric_history[ActivityMetricStepCount], complete_history,
         sizeof(complete_history));

  ActivityInsightMetricHistoryStats stats;
  prv_calculate_metric_history_stats(ActivityMetricStepCount, &stats);
  cl_assert_equal_i(stats.median, 5079);
  cl_assert_equal_i(stats.total_days, 29);
  cl_assert_equal_i(stats.consecutive_days, 29);

  // Test sparse history
  static const int32_t sparse_history[ACTIVITY_HISTORY_DAYS] = {
    1234, // This value is ignored since it's loaded in as the current day
    6233, 4277, 9857, 0, 6540, 719, 0, 0, 0, 0, 0, 0, 0, 0, 6664, 9177, 0, 2330, 3951, 1568,
    871, 0, 8751, 0, 7813, 772, 0, 7438, 428
  };
  memcpy(&s_data.metric_history[ActivityMetricStepCount], sparse_history,
         sizeof(sparse_history));
  prv_calculate_metric_history_stats(ActivityMetricStepCount, &stats);
  cl_assert_equal_i(stats.median, 4277);
  cl_assert_equal_i(stats.total_days, 16);
  cl_assert_equal_i(stats.consecutive_days, 3);
}

// ---------------------------------------------------------------------------------------
// Test that the sleep reward triggers when it should, and doesn't trigger when it shouldn't
void test_activity_insights__sleep_reward(void) {
  // Use reasonable insight settings
  const ActivityScalarStore AVERAGE_SLEEP = 5 * MINUTES_PER_HOUR;
  const ActivityScalarStore GOOD_SLEEP = 8 * MINUTES_PER_HOUR;

  static const int32_t sleep_history[ACTIVITY_HISTORY_DAYS] = {
    GOOD_SLEEP,     // This is 'today'
    GOOD_SLEEP,     // User has had good sleep for past 3 nights
    GOOD_SLEEP,
    GOOD_SLEEP,
    AVERAGE_SLEEP,  // Average sleep to make sure our median is fairly low
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP
  };
  memcpy(&s_data.metric_history[ActivityMetricSleepTotalSeconds], sleep_history,
         sizeof(sleep_history));

  activity_insights_init(rtc_get_time());

  // Make sure we don't trigger while still asleep
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateLightSleep;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  // Make sure we don't trigger as soon as we're awake
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 1 * SECONDS_PER_HOUR;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  // Make sure we do not trigger, the insights are disabled
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 2 * SECONDS_PER_HOUR;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  // Advance the clock some and make sure we still don't get notifications
  for (int i = 0; i < 100; ++i) {
    rtc_set_time(rtc_get_time() + 2 * SECONDS_PER_MINUTE);
    activity_insights_process_sleep_data(rtc_get_time());
    cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
  }

//  // These tests only make sense if the insights are enabled
//  // Now we shouldn't see another notification for the next 6 days
//  for (int i = 0; i < 6; ++i) {
//    rtc_set_time(rtc_get_time() + SECONDS_PER_DAY);
//    activity_insights_recalculate_stats();
//    activity_insights_process_sleep_data(rtc_get_time());
//    cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 1);
//  }
//
//  rtc_set_time(rtc_get_time() + SECONDS_PER_DAY);
//  activity_insights_recalculate_stats();
//  activity_insights_process_sleep_data(rtc_get_time());
//  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 2);
//
//  // Make sure we don't trigger if we didn't get enough sleep
//  rtc_set_time(rtc_get_time() + 7 * SECONDS_PER_DAY);
//  activity_insights_recalculate_stats();
//  s_data.metric_history[ActivityMetricSleepTotalSeconds][0] = AVERAGE_SLEEP;
//  activity_insights_process_sleep_data(rtc_get_time());
//  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 2);
//
//  // Fall back asleep, make sure we get the reward
//  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateLightSleep;
//  activity_insights_process_sleep_data(rtc_get_time());
//  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
//  s_data.metric_history[ActivityMetricSleepTotalSeconds][0] = GOOD_SLEEP;
//  activity_insights_process_sleep_data(rtc_get_time());
//  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 3);
//
//  // Make sure setting enable to false actually disables things
//  rtc_set_time(rtc_get_time() + 7 * SECONDS_PER_DAY);
//  activity_insights_recalculate_stats();
//  ActivityInsightSettings disabled_sleep;
//  activity_insights_settings_read(ACTIVITY_INSIGHTS_SETTINGS_SLEEP_REWARD, &disabled_sleep);
//  disabled_sleep.enabled = false,
//  settings_file_set(NULL, /* fake settings file don't care */
//                    ACTIVITY_INSIGHTS_SETTINGS_SLEEP_REWARD, strlen(ACTIVITY_INSIGHTS_SETTINGS_SLEEP_REWARD),
//                    &disabled_sleep, sizeof(disabled_sleep));
//  pfs_watch_cb(NULL); // Update the settings cache
//  activity_insights_process_sleep_data(rtc_get_time());
//  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 3);
}

static void prv_minute_update(int iterations) {
  for ( ; iterations > 0; --iterations) {
    rtc_set_time(rtc_get_time() + SECONDS_PER_MINUTE);
    activity_insights_process_minute_data(rtc_get_time());
  }
}

// ---------------------------------------------------------------------------------------
void prv_set_step_history_avg() {
  // History with low median
  static const int32_t step_history[ACTIVITY_HISTORY_DAYS] = {
    AVERAGE_STEPS, // This is 'today'
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS,
    AVERAGE_STEPS
  };
  memcpy(&s_data.metric_history[ActivityMetricStepCount], &step_history, sizeof(step_history));
}

void prv_set_sleep_history_avg() {
  const ActivityScalarStore AVERAGE_SLEEP = 5 * MINUTES_PER_HOUR;

  static const int32_t sleep_history[ACTIVITY_HISTORY_DAYS] = {
    AVERAGE_SLEEP,  // This is 'today'
    AVERAGE_SLEEP,  // Average sleep to make sure our median is fairly low
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP,
    AVERAGE_SLEEP
  };
  memcpy(&s_data.metric_history[ActivityMetricSleepTotalSeconds], sleep_history,
         sizeof(sleep_history));
}

// ---------------------------------------------------------------------------------------
// Test that the activity reward triggers when it should, and doesn't trigger when it shouldn't
void test_activity_insights__activity_reward_no_trigger_default_state(void) {
  prv_set_step_history_avg();
  activity_insights_init(rtc_get_time());

  // Make sure we don't trigger in the default state (not above median, not active)
  s_data.steps_per_minute = 0;
  s_data.metric_history[ActivityMetricStepCount][0] = AVERAGE_STEPS;
  prv_minute_update(ACTIVE_MINUTES);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
}

void test_activity_insights__activity_reward_no_trigger_below_avg(void) {
  prv_set_step_history_avg();
  activity_insights_init(rtc_get_time());

  // Make sure that when we are active, we don't trigger without being above average
  s_data.steps_per_minute = 80;
  s_data.metric_history[ActivityMetricStepCount][0] = AVERAGE_STEPS;
  prv_minute_update(ACTIVE_MINUTES);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
}

void test_activity_insights__activity_reward_no_trigger_not_active(void) {
  prv_set_step_history_avg();
  activity_insights_init(rtc_get_time());

  // Make sure that being above average but not active doesn't trigger
  s_data.steps_per_minute = 0;
  s_data.metric_history[ActivityMetricStepCount][0] = HIGH_STEPS;
  prv_minute_update(ACTIVE_MINUTES);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
}

void test_activity_insights__activity_reward_trigger(void) {
  prv_set_step_history_avg();
  activity_insights_init(rtc_get_time());

  // This would trigger the insights if they weren't disabled
  s_data.steps_per_minute = 80;
  s_data.metric_history[ActivityMetricStepCount][0] = HIGH_STEPS;
  prv_minute_update(ACTIVE_MINUTES);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

//  // This tests multi day triggers if insights are enabled
//  prv_minute_update(ACTIVE_MINUTES);
//  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
//  rtc_set_time(rtc_get_time() + 1 * SECONDS_PER_DAY);
//  activity_insights_recalculate_stats();
//  prv_minute_update(ACTIVE_MINUTES);
//  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
}

void test_activity_insights__disable_activity_reward(void) {
  prv_set_step_history_avg();
  activity_insights_init(rtc_get_time());

  // Set up criteria to trigger reward
  s_data.steps_per_minute = 80;
  s_data.metric_history[ActivityMetricStepCount][0] = HIGH_STEPS;

  // Make sure setting enable to false actually disables things
  activity_insights_recalculate_stats();
  ActivityInsightSettings disabled_activity;
  activity_insights_settings_read(ACTIVITY_INSIGHTS_SETTINGS_ACTIVITY_REWARD, &disabled_activity);
  disabled_activity.enabled = false,
  settings_file_set(NULL, /* fake settings file don't care */
                    ACTIVITY_INSIGHTS_SETTINGS_ACTIVITY_REWARD,
                    strlen(ACTIVITY_INSIGHTS_SETTINGS_ACTIVITY_REWARD), &disabled_activity,
                    sizeof(disabled_activity));
  pfs_watch_cb(NULL); // Update the settings cache
  prv_minute_update(ACTIVE_MINUTES);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);
}

// ---------------------------------------------------------------------------------------
// Make sure we don't push an activity pin when we have no history to compare against
void test_activity_insights__activity_summary_no_history(void) {
  // Tests init with zero history

  // Set time to be after 8:30PM
  rtc_set_time((20 * SECONDS_PER_HOUR) + (40 * SECONDS_PER_MINUTE));

  activity_insights_init(rtc_get_time());

  // Provide non-zero step count
  s_data.metric_history[ActivityMetricStepCount][0] = 500;
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 0);
}

// ---------------------------------------------------------------------------------------
void test_activity_insights__sleep_summary(void) {
  // Use reasonable insight settings
  prv_set_sleep_history_avg();

  // Let's start at 11:30pm
  struct tm start_tm = {
    // Thursday, Jan 1, 2015, 11:30pm
    .tm_hour = 23,
    .tm_min = 30,
    .tm_mday = 1,
    .tm_mon = 0,
    .tm_year = 115
  };
  prv_set_time(&start_tm);
  activity_insights_init(rtc_get_time());

  // Make sure we don't trigger while still asleep
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateLightSleep;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 0);

  // Put in a 1 hour sleep session that ends at 11pm. This is after
  // ACTIVITY_LAST_SLEEP_MINUTE_OF_DAY (9pm), so it should be part of "tonight's" sleep.
  prv_add_sleep_session(22, 1);     // Starting 22 hours from midnight of today

  // Awake until 11:45pm
  rtc_set_time(rtc_get_time() + 15 * SECONDS_PER_MINUTE);
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 15 * SECONDS_PER_MINUTE;

  // Should generate a pin
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);
  Uuid orig_id = s_last_timeline_id;

  // Advance to midnight and perform the midnight rollover logic
  rtc_set_time(rtc_get_time() + 15 * SECONDS_PER_MINUTE);  // Puts us at midnight
  activity_insights_recalculate_stats();                   // Process the midnight rollover logic

  // Advance to 7:05am and add a sleep session from midnight to 7am
  rtc_set_time(rtc_get_time() + 7 * SECONDS_PER_HOUR + 5 * SECONDS_PER_MINUTE);  // Puts us at 7:05
  prv_add_sleep_session(0, 7);

  // Make sure we update the existing pin as soon as we are awake. We shouldn't add another pin
  // because the sleep includes all sleep since ACTIVITY_LAST_SLEEP_MINUTE_OF_DAY (9pm) the prior
  // day.
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 5 * SECONDS_PER_MINUTE;
  activity_insights_process_sleep_data(rtc_get_time());
  // Pin added should have been called again, but with the same UUID
  cl_assert_equal_i(s_data.pins_added, 2);
  cl_assert(uuid_equal(&orig_id, &s_last_timeline_id));

  // Make sure we don't trigger again for the same sleep session
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 60 * SECONDS_PER_MINUTE;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 2);
}

// ---------------------------------------------------------------------------------------
// This makes sure the sleep summary properly handles the midnight rollover for non-UTC timezones
// since when there's no sleep, the enter/exit times will be set to midnight UTC and the metrics
// will return that time in localtime
void test_activity_insights__sleep_summary_midnight_timezone(void) {
  // Set to a non-UTC timezone
  static TimezoneInfo tz = {
    .tm_gmtoff = -8 * SECONDS_PER_HOUR, // PST
  };
  time_util_update_timezone(&tz);
  rtc_set_time(time_util_get_midnight_of(rtc_get_time()) + tz.tm_gmtoff);

  prv_set_sleep_history_avg();

  activity_insights_init(rtc_get_time());

  // At midnight, enter/exit get set to midnight UTC (for PST, this is 4PM), total sleep is 0
  uint32_t midnight_local = ((24 + tz.tm_gmtoff) % 24) * SECONDS_PER_HOUR;

  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 30 * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepExitAtSeconds][0] = midnight_local;
  s_data.metric_history[ActivityMetricSleepEnterAtSeconds][0] = midnight_local;
  s_data.metric_history[ActivityMetricSleepTotalSeconds][0] = 0;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 0);

  // Make sure pin is pushed once we have slept some
  prv_add_sleep_session(0, 7);
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);
}

// ---------------------------------------------------------------------------------------
// Make sure that if you wake up for a short period of time, we'll move the sleep pin
void test_activity_insights__sleep_summary_merge(void) {
  prv_set_sleep_history_avg();
  activity_insights_init(rtc_get_time());

  // Common metrics
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 30 * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepEnterAtSeconds][0] = 0;

  // First session should always produce a pin
  prv_add_sleep_session(0, 7);
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);
  Uuid orig_id = s_last_timeline_id;

  // Next session, < 1h after should move the pin
  rtc_set_time(rtc_get_time() + 2 * SECONDS_PER_HOUR);
  prv_add_sleep_session(0.5, 1.5);
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 2);
  cl_assert(uuid_equal(&orig_id, &s_last_timeline_id));

  // Nap sessions shouldn't be added
  rtc_set_time(rtc_get_time() + 3 * SECONDS_PER_HOUR);
  prv_add_nap_session(2, 1);
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 2);
}

// ---------------------------------------------------------------------------------------
// Make sure that when the watch resets, we retain state properly
void test_activity_insights__sleep_summary_power_cycle(void) {
  prv_set_sleep_history_avg();
  activity_insights_init(rtc_get_time());

  // Common metrics
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 30 * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepEnterAtSeconds][0] = 0;

  // Push a pin
  rtc_set_time(rtc_get_time() + 5 * SECONDS_PER_HOUR);
  prv_add_sleep_session(0, 7);
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);

  // Re-init (simulates power cycle) and make sure we don't add a pin again
  activity_insights_init(rtc_get_time());
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);

  // Make sure we still merge properly after a power cycle
  activity_insights_init(rtc_get_time());
  rtc_set_time(rtc_get_time() + 2 * SECONDS_PER_HOUR);
  prv_add_sleep_session(0.5, 1.5);
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 2);
}

// ---------------------------------------------------------------------------------------
// Make sure we don't push a pin when we have no history to compare against
void test_activity_insights__sleep_summary_no_history(void) {
  // Tests init with zero history

  activity_insights_init(rtc_get_time());

  // Make sure we don't trigger as soon as we're awake
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepExitAtSeconds][0] = 7 * SECONDS_PER_HOUR;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 30 * SECONDS_PER_MINUTE;
  activity_insights_process_sleep_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 0);
}

// ---------------------------------------------------------------------------------------
void test_activity_insights__activation_delay_insights_time_trigger(void) {
  time_t now = mktime(&s_init_time_tm);
  prv_set_activation_time(now);

  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  now += SECONDS_PER_DAY; // Jan 2 @ 10:00am
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  now += 8 * SECONDS_PER_HOUR; // Jan 2 @ 6:00pm
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 1);

  now += (3 * SECONDS_PER_DAY) + (2 * SECONDS_PER_HOUR); // Jan 5 @ 8:00pm
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 1);

  s_health_app_opened_version = 1;

  now += 30 * SECONDS_PER_MINUTE; // Jan 5 @ 8:30pm
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 2);

  now += 6 * SECONDS_PER_DAY; // Jan 11 @ 8:30pm
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 3);
}

// ---------------------------------------------------------------------------------------
void test_activity_insights__activation_delay_insights_fifteen_interval_trigger(void) {
  time_t now = mktime(&s_init_time_tm);
  prv_set_activation_time(now);

  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  now += SECONDS_PER_DAY + (8 * SECONDS_PER_HOUR) + (5 * SECONDS_PER_MINUTE); // Jan 2 @ 6:05pm
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 0);

  now += (10 * SECONDS_PER_MINUTE); // Jan 2 @ 6:15pm
  rtc_set_time(now);
  activity_insights_process_minute_data(now);
  cl_assert_equal_i(fake_kernel_services_notifications_ancs_notifications_count(), 1);
}

// Make sure that when the watch resets, we retain state properly
void test_activity_insights__nap_session_power_cycle(void) {
  // PBL-36355 Disable nap notifications
  // Enable this unit test when re-enabling nap session notifications
#if 0
  prv_set_sleep_history_avg();
  activity_insights_init(rtc_get_time());

  // Common metrics
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 30 * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepEnterAtSeconds][0] = 0;

  // Push a pin
  rtc_set_time(rtc_get_time() + 5 * SECONDS_PER_HOUR);
  prv_add_nap_session(0, 1);
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);

  // Re-init (simulates power cycle) and make sure we don't add a pin again
  activity_insights_init(rtc_get_time());
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 1);

  // Make sure we still trigger properly after a power cycle
  activity_insights_init(rtc_get_time());
  rtc_set_time(rtc_get_time() + 2 * SECONDS_PER_HOUR);
  prv_add_nap_session(0.5, 1.5);
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.pins_added, 2);
#endif
}

// Make sure that when the watch resets, we retain state properly
void test_activity_insights__walk_session_power_cycle(void) {
  activity_insights_init(rtc_get_time());
  // Common metrics
  s_data.metric_history[ActivityMetricSleepState][0] = ActivitySleepStateAwake;
  s_data.metric_history[ActivityMetricSleepStateSeconds][0] = 30 * SECONDS_PER_MINUTE;
  s_data.metric_history[ActivityMetricSleepEnterAtSeconds][0] = 0;

  // Push a pin
  rtc_set_time(rtc_get_time() + 5 * SECONDS_PER_HOUR);
  prv_add_walk_session(0, 1);
  rtc_set_time(rtc_get_time() + 2 * SECONDS_PER_HOUR);
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.notifs_shown, 1);

  // Re-init (simulates power cycle) and make sure we don't add a pin again
  activity_insights_init(rtc_get_time());
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.notifs_shown, 1);

  // Make sure we still trigger properly after a power cycle
  activity_insights_init(rtc_get_time());
  prv_add_walk_session(0, 1);
  rtc_set_time(rtc_get_time() + 2 * SECONDS_PER_HOUR);
  activity_insights_process_minute_data(rtc_get_time());
  cl_assert_equal_i(s_data.notifs_shown, 2);
}
