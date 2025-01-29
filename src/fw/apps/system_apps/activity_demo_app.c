/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "applib/health_service.h"
#include "applib/app.h"
#include "applib/app_logging.h"
#include "applib/fonts/fonts.h"
#include "applib/persist.h"
#include "applib/ui/ui.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "apps/system_app_ids.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "services/normal/activity/activity_algorithm.h"
#include "services/normal/activity/activity_insights.h"
#include "services/normal/data_logging/data_logging_service.h"
#include "shell/prefs.h"
#include "system/logging.h"
#include "util/size.h"
#include "util/string.h"
#include "util/trig.h"

#include "activity_demo_app.h"

#include <stdio.h>

#define CURRENT_STEP_AVG 500
#define DAILY_STEP_AVG 1000

// Persist keys
typedef enum {
  AppPersistKeyLapSteps = 0,
} AppPersistKey;



// -------------------------------------------------------------------------------
// Structures

typedef struct {
  char dialog_text[256];
  SimpleMenuItem *menu_items;
  SimpleMenuLayer *menu_layer;
} DebugCard;

// App globals
typedef struct {
  Window *debug_window;
  DebugCard debug_card;
  uint32_t steps_offset;
  uint32_t cur_steps;
} ActivityDemoAppData;

static ActivityDemoAppData *s_data;

// -------------------------------------------------------------------------------
static void prv_convert_seconds_to_time(uint32_t secs_after_midnight, char *text,
                                        int text_len) {
  uint32_t minutes_after_midnight = secs_after_midnight / SECONDS_PER_MINUTE;
  uint32_t hour = minutes_after_midnight / MINUTES_PER_HOUR;
  uint32_t minute = minutes_after_midnight % MINUTES_PER_HOUR;
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(text, text_len, "%d:%02d", (int)hour, (int)minute);
}

// -----------------------------------------------------------------------------------------
static void prv_display_alert(const char *text) {
  ExpandableDialog *expandable_dialog = expandable_dialog_create("Alert");

  dialog_set_text(expandable_dialog_get_dialog(expandable_dialog), text);
  expandable_dialog_show_action_bar(expandable_dialog, false);
  app_expandable_dialog_push(expandable_dialog);
}


// -----------------------------------------------------------------------------------------
static void prv_display_scalar_history_alert(ActivityDemoAppData *data, const char *title,
                                             ActivityMetric metric) {
  strcpy(data->debug_card.dialog_text, title);

  // Get History
  int32_t values[7];
  activity_get_metric(metric, ARRAY_LENGTH(values), values);
  for (int i = 0; i < 7; i++) {
    char temp[32];
    snprintf(temp, sizeof(temp), "\n%d: %d", i, (int)values[i]);
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));
  }
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_display_averages_alert(ActivityDemoAppData *data, DayInWeek day) {
  ActivityMetricAverages *averages = app_malloc_check(sizeof(ActivityMetricAverages));
  strcpy(data->debug_card.dialog_text, "Hourly avgs:");
  activity_get_step_averages(day, averages);

  // Sum into hours
  const int k_avgs_per_hour = ACTIVITY_NUM_METRIC_AVERAGES / HOURS_PER_DAY;
  for (int i = 0; i < HOURS_PER_DAY; i++) {
    int value = 0;
    for (int j = i * k_avgs_per_hour; j < (i + 1) * k_avgs_per_hour; j++) {
      if (averages->average[j] == ACTIVITY_METRIC_AVERAGES_UNKNOWN) {
        averages->average[j] = 0;
      }
      value += averages->average[j];
    }
    char temp[32];
    snprintf(temp, sizeof(temp), "\n%02d: %d", i, value);
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));
  }
  prv_display_alert(data->debug_card.dialog_text);
  app_free(averages);
}


// -----------------------------------------------------------------------------------------
static void prv_display_seconds_history_alert(ActivityDemoAppData *data, const char *title,
                                              ActivityMetric metric) {
  strcpy(data->debug_card.dialog_text, title);

  // Get History
  int32_t values[7];
  activity_get_metric(metric, ARRAY_LENGTH(values), values);
  for (int i = 0; i < 7; i++) {
    char elapsed[8];
    prv_convert_seconds_to_time(values[i], elapsed, sizeof(elapsed));
    char temp[32];
    snprintf(temp, sizeof(temp), "\n%d: %s", i, elapsed);
    strcat(data->debug_card.dialog_text, temp);
  }
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_set_steps(int32_t steps, ActivityDemoAppData *data) {
  activity_test_set_steps_and_avg(steps, CURRENT_STEP_AVG, DAILY_STEP_AVG);

  int32_t peek_steps = health_service_sum_today(HealthMetricStepCount);
  data->cur_steps = peek_steps + data->steps_offset;

  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Current steps changed to: %"PRIu32"\n", data->cur_steps);
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_tracking(int index, void *context) {
  ActivityDemoAppData *data = context;

  bool enabled = activity_tracking_on();
  enabled = !enabled;

  if (enabled) {
    activity_start_tracking(false /*test_mode*/);
  } else {
    activity_stop_tracking();
  }
  activity_prefs_tracking_set_enabled(enabled);

  data->debug_card.menu_items[index].subtitle = enabled ? "Enabled" : "Disabled";
  layer_mark_dirty(simple_menu_layer_get_layer(data->debug_card.menu_layer));
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_activity_insights(int index, void *context) {
  ActivityDemoAppData *data = context;

  bool enabled = activity_prefs_activity_insights_are_enabled();
  enabled = !enabled;
  activity_prefs_activity_insights_set_enabled(enabled);

  data->debug_card.menu_items[index].subtitle = enabled ? "Enabled" : "Disabled";
  layer_mark_dirty(simple_menu_layer_get_layer(data->debug_card.menu_layer));
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_sleep_insights(int index, void *context) {
  ActivityDemoAppData *data = context;

  bool enabled = activity_prefs_sleep_insights_are_enabled();
  enabled = !enabled;
  activity_prefs_sleep_insights_set_enabled(enabled);

  data->debug_card.menu_items[index].subtitle = enabled ? "Enabled" : "Disabled";
  layer_mark_dirty(simple_menu_layer_get_layer(data->debug_card.menu_layer));
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_dls_sends(int index, void *context) {
  ActivityDemoAppData *data = context;

  bool enabled = dls_get_send_enable();
  enabled = !enabled;
  dls_set_send_enable_pp(enabled);

  data->debug_card.menu_items[index].subtitle = enabled ? "Enabled" : "Disabled";
  layer_mark_dirty(simple_menu_layer_get_layer(data->debug_card.menu_layer));
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_set_steps_below_avg(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_set_steps(CURRENT_STEP_AVG - 250, data);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_set_steps_at_avg(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_set_steps(CURRENT_STEP_AVG, data);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_set_steps_above_avg(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_set_steps(CURRENT_STEP_AVG + 250, data);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_set_steps_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  activity_test_set_steps_history();

  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Step history changed");
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_set_sleep_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  activity_test_set_sleep_history();

  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Sleep history changed");
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_sleep_file_info(int index, void *context) {
  ActivityDemoAppData *data = context;

  // Get sleep file info
  uint32_t num_records;
  uint32_t data_bytes;
  uint32_t minutes;
  activity_test_minute_file_info(false /*compact_first*/, &num_records, &data_bytes, &minutes);
  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Records: %d\nData bytes: %d\nMinutes: %d", (int)num_records, (int)data_bytes,
           (int)minutes);
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_sleep_file_compact(int index, void *context) {
  ActivityDemoAppData *data = context;

  // Get sleep file info
  uint32_t num_records;
  uint32_t data_bytes;
  uint32_t minutes;
  activity_test_minute_file_info(true /*compact_first*/, &num_records, &data_bytes, &minutes);
  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "After compaction\nRecords: %d\nData bytes: %d\nMinutes: %d", (int)num_records,
           (int)data_bytes, (int)minutes);
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_resting_calorie_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_scalar_history_alert(data, "Resting Calories", ActivityMetricRestingKCalories);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_active_calorie_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_scalar_history_alert(data, "Active Calories", ActivityMetricActiveKCalories);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_step_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_scalar_history_alert(data, "Steps", ActivityMetricStepCount);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_sleep_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_seconds_history_alert(data, "Sleep total", ActivityMetricSleepTotalSeconds);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_active_time_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_seconds_history_alert(data, "Active Time", ActivityMetricActiveSeconds);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_distance_history(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_scalar_history_alert(data, "Distance(m)", ActivityMetricDistanceMeters);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_sleep_sessions(int index, void *context) {
  ActivityDemoAppData *data = context;

  // Allocate space for the sessions. Usually, there will only be about 4 or 5 sessions
  // (1 container and a handful of restful periods). Allocating space for 32 (an arbitrary
  // number) should be more than enough.
  uint32_t num_sessions = 32;
  ActivitySession *sessions = app_malloc(num_sessions * sizeof(ActivitySession));
  if (!sessions) {
    snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
             "Not enough memory");
    return;
  }

  // Get sessions
  bool success = activity_get_sessions(&num_sessions, sessions);
  if (!success) {
    snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Error getting sleep sessions");
    goto exit;
  }
  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Sleep sessions\n");

  // Print info on each one
  ActivitySession *session = sessions;
  for (uint32_t i = 0; i < num_sessions; i++, session++) {
    char *prefix = "";
    bool deep_sleep = false;
    switch (session->type) {
      case ActivitySessionType_Sleep:
        prefix = "s";
        break;
      case ActivitySessionType_Nap:
        prefix = "n";
        break;
      case ActivitySessionType_RestfulSleep:
      case ActivitySessionType_RestfulNap:
        prefix = "*";
        deep_sleep = true;
        break;
      default:
        continue;
    }
    safe_strcat(data->debug_card.dialog_text, prefix, sizeof(data->debug_card.dialog_text));

    // Write start time
    struct tm local_tm;
    char temp[32];
    localtime_r(&session->start_utc, &local_tm);
    strftime(temp, sizeof(temp), "%H:%M", &local_tm);
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));

    // Write end time/elapsed
    if (deep_sleep) {
      snprintf(temp, sizeof(temp), " %dm\n", (int)(session->length_min));
    } else {
      time_t end_time = session->start_utc + (session->length_min * SECONDS_PER_MINUTE);
      localtime_r(&end_time, &local_tm);
      strftime(temp, sizeof(temp), "-%H:%M\n", &local_tm);
    }
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));
  }

exit:
  // Free session info memory
  app_free(sessions);
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_step_sessions(int index, void *context) {
  ActivityDemoAppData *data = context;

  // Allocate space for the sessions. Usually, there will only be about 4 or 5 sessions
  // (1 container and a handful of restful periods). Allocating space for 32 (an arbitrary
  // number) should be more than enough.
  uint32_t num_sessions = 32;
  ActivitySession *sessions = app_malloc(num_sessions * sizeof(ActivitySession));
  if (!sessions) {
    snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
             "Not enough memory");
    return;
  }

  // Get sessions
  bool success = activity_get_sessions(&num_sessions, sessions);
  if (!success) {
    snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
             "Error getting activity sessions");
    goto exit;
  }
  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Step activities\n");

  // Print info on each one
  ActivitySession *session = sessions;
  for (uint32_t i = 0; i < num_sessions; i++, session++) {
    char *prefix = "";
    switch (session->type) {
      case ActivitySessionType_Sleep:
      case ActivitySessionType_Nap:
      case ActivitySessionType_RestfulSleep:
      case ActivitySessionType_RestfulNap:
        continue;
      case ActivitySessionType_Walk:
        prefix = "W";
        break;
      case ActivitySessionType_Run:
        prefix = "R";
        break;
      default:
        continue;
    }
    safe_strcat(data->debug_card.dialog_text, prefix, sizeof(data->debug_card.dialog_text));

    // Write start time
    struct tm local_tm;
    char temp[64];
    localtime_r(&session->start_utc, &local_tm);
    strftime(temp, sizeof(temp), "%H:%M", &local_tm);
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));

    // Write length
    snprintf(temp, sizeof(temp), " %dm\n", (int)(session->length_min));
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));

    // Write steps, calories, distance
    snprintf(temp, sizeof(temp), " %"PRIu16", %"PRIu16"C, %"PRIu16"m\n", session->step_data.steps,
             session->step_data.active_kcalories + session->step_data.resting_kcalories,
             session->step_data.distance_meters);
    safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));
  }

  exit:
  // Free session info memory
  app_free(sessions);
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_weekday_averages(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_averages_alert(data, Monday);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_weekend_averages(int index, void *context) {
  ActivityDemoAppData *data = context;
  prv_display_averages_alert(data, Saturday);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_activity_prefs(int index, void *context) {
  ActivityDemoAppData *data = context;
  bool tracking_enabled = activity_prefs_tracking_is_enabled();
  bool activity_insights_enabled = activity_prefs_activity_insights_are_enabled();
  bool sleep_insights_enabled = activity_prefs_sleep_insights_are_enabled();
  ActivityGender gender = activity_prefs_get_gender();
  uint16_t height_mm = activity_prefs_get_height_mm();
  uint16_t weight_dag = activity_prefs_get_weight_dag();
  uint8_t age_years = activity_prefs_get_age_years();

  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
      "activity tracking: %"PRIu8"\n"
      "activity_insights: %"PRIu8"\n"
      "sleep_insights: %"PRIu8"\n"
      "gender: %"PRIu8"\n"
      "height: %"PRIu16"\n"
      "weight: %"PRIu16"\n"
      "age: %"PRIu8"", tracking_enabled, activity_insights_enabled, sleep_insights_enabled,
      gender, height_mm, weight_dag, age_years);
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_minute_data(int index, void *context) {
  ActivityDemoAppData *data = context;
  bool success;

  const uint32_t k_size = 1000;
  HealthMinuteData *minute_data = app_malloc(k_size * sizeof(HealthMinuteData));
  if (!minute_data) {
    snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
             "Out of memory");
    prv_display_alert(data->debug_card.dialog_text);
    goto exit;
  }


  // Start as far back as 30 days ago
  time_t utc_start = rtc_get_time() - 30 * SECONDS_PER_DAY;
  uint32_t num_records = 0;
  while (true) {
    uint32_t chunk = k_size;
    time_t prior_start = utc_start;
    success = activity_get_minute_history(minute_data, &chunk, &utc_start);
    if (!success) {
      snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
               "Failed");
      prv_display_alert(data->debug_card.dialog_text);
      goto exit;
    }
    PBL_LOG(LOG_LEVEL_DEBUG, "Got %d minutes with UTC of %d (delta of %d min)", (int)chunk,
            (int)utc_start, (int)(utc_start - prior_start) / SECONDS_PER_MINUTE);
    num_records += chunk;
    utc_start += chunk * SECONDS_PER_MINUTE;
    if (chunk == 0) {
      break;
    }
  }

  // Print summary
  snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
           "Retrieved %d minute data records", (int)num_records);


  // Print detail on the last few minutes
  const int k_print_batch_size = k_size;
  PBL_LOG(LOG_LEVEL_DEBUG, "Fetching last %d minutes", k_print_batch_size);
  utc_start = rtc_get_time() - (k_print_batch_size * SECONDS_PER_MINUTE);
  time_t prior_start = utc_start;
  uint32_t chunk = k_print_batch_size;
  success = activity_get_minute_history(minute_data, &chunk, &utc_start);
  if (!success) {
    snprintf(data->debug_card.dialog_text, sizeof(data->debug_card.dialog_text),
             "Failed");
    prv_display_alert(data->debug_card.dialog_text);
    goto exit;
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Got last %d minutes with UTC of %d (delta of %d min)", (int)chunk,
          (int)utc_start, (int)(utc_start - prior_start) / SECONDS_PER_MINUTE);

  const unsigned int k_num_last_minutes = 6;
  if (chunk >= k_num_last_minutes) {
    for (int i = (int)chunk - k_num_last_minutes; i < (int)chunk; i++) {
      HealthMinuteData *m_data = &minute_data[i];
      char temp[32];
      snprintf(temp, sizeof(temp), "\n%"PRId8", 0x%"PRIx8", %"PRIu16", %"PRId8" ",
               m_data->steps, m_data->orientation, m_data->vmc, m_data->light);
      safe_strcat(data->debug_card.dialog_text, temp, sizeof(data->debug_card.dialog_text));
    }
  }

  prv_display_alert(data->debug_card.dialog_text);

exit:
  app_free(minute_data);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_send_fake_logging_record(int index, void *context) {
  activity_test_send_fake_dls_records();
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_push_summary_pins(int index, void *context) {
  prv_debug_cmd_set_steps_at_avg(index, context);

  activity_insights_test_push_summary_pins();

  ActivityDemoAppData *data = context;
  strncpy(data->debug_card.dialog_text, "Summary pins pushed",
          sizeof(data->debug_card.dialog_text));
  prv_display_alert(data->debug_card.dialog_text);
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_push_rewards(int index, void *context) {
  activity_insights_test_push_rewards();
}


// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_push_walk_run(int index, void *context) {
  activity_insights_test_push_walk_run_sessions();
}

// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_push_day_insights(int index, void *context) {
  activity_insights_test_push_day_insights();
}

// -----------------------------------------------------------------------------------------
static void prv_debug_cmd_push_nap_session(int index, void *context) {
  activity_insights_test_push_nap_session();
}

// -----------------------------------------------------------------------------------------
static void debug_window_load(Window *window) {
  ActivityDemoAppData *data = window_get_user_data(window);
  Layer *window_layer = window_get_root_layer(window);
  const GRect *root_bounds = &window_layer->bounds;

  static SimpleMenuItem menu_items[] = {
    {
      .title = "Tracking",
      .callback = prv_debug_cmd_tracking,
    }, {
      .title = "Activity Insights",
      .callback = prv_debug_cmd_activity_insights,
    }, {
      .title = "Sleep Insights",
      .callback = prv_debug_cmd_sleep_insights,
    }, {
      .title = "DLS sends",
      .callback = prv_debug_cmd_dls_sends,
    }, {
      .title = "Step History",
      .callback = prv_debug_cmd_step_history,
    }, {
      .title = "Distance(m) History",
      .callback = prv_debug_cmd_distance_history,
    }, {
      .title = "Resting Calorie History",
      .callback = prv_debug_cmd_resting_calorie_history,
    }, {
      .title = "Active Calorie History",
      .callback = prv_debug_cmd_active_calorie_history,
    }, {
      .title = "Active Minutes History",
      .callback = prv_debug_cmd_active_time_history,
    }, {
      .title = "Sleep History",
      .callback = prv_debug_cmd_sleep_history,
    }, {
      .title = "Sleep Sessions",
      .callback = prv_debug_cmd_sleep_sessions,
    }, {
      .title = "Step activities",
      .callback = prv_debug_cmd_step_sessions,
    }, {
      .title = "Weekday averages",
      .callback = prv_debug_cmd_weekday_averages,
    }, {
      .title = "Weekend averages",
      .callback = prv_debug_cmd_weekend_averages,
    }, {
      .title = "Activity Prefs",
      .callback = prv_debug_cmd_activity_prefs,
    }, {
      .title = "Steps below avg",
      .callback = prv_debug_cmd_set_steps_below_avg,
    }, {
      .title = "Steps at avg",
      .callback = prv_debug_cmd_set_steps_at_avg,
    }, {
      .title = "Steps above avg",
      .callback = prv_debug_cmd_set_steps_above_avg,
    }, {
      .title = "Set step history",
      .callback = prv_debug_cmd_set_steps_history,
    }, {
      .title = "Set sleep history",
      .callback = prv_debug_cmd_set_sleep_history,
    }, {
      .title = "Sleep File Info",
      .callback = prv_debug_cmd_sleep_file_info,
    }, {
      .title = "Sleep File Compact",
      .callback = prv_debug_cmd_sleep_file_compact,
    }, {
      .title = "Read Minute data",
      .callback = prv_debug_cmd_minute_data,
    }, {
      .title = "Send fake DL record",
      .callback = prv_debug_cmd_send_fake_logging_record,
    }, {
      .title = "Push Summary Pins",
      .callback = prv_debug_cmd_push_summary_pins,
    }, {
      .title = "Push Rewards",
      .callback = prv_debug_cmd_push_rewards,
    }, {
      .title = "Walk/Run Notif",
      .callback = prv_debug_cmd_push_walk_run,
    }, {
      .title = "Push Day Insights",
      .callback = prv_debug_cmd_push_day_insights,
    }, {
      .title = "Push Nap Session",
      .callback = prv_debug_cmd_push_nap_session,
    }
  };
  static const SimpleMenuSection sections[] = {
    {
      .items = menu_items,
      .num_items = ARRAY_LENGTH(menu_items)
    }
  };

  data->debug_card.menu_items = menu_items;
  data->debug_card.menu_layer = simple_menu_layer_create(*root_bounds, window, sections,
                                                         ARRAY_LENGTH(sections), data);
  layer_add_child(window_layer, simple_menu_layer_get_layer(data->debug_card.menu_layer));

  // Init status
  data->debug_card.menu_items[0].subtitle = activity_tracking_on() ? "Enabled" : "Disabled";
  data->debug_card.menu_items[1].subtitle =
      activity_prefs_activity_insights_are_enabled() ? "Enabled" : "Disabled";
  data->debug_card.menu_items[2].subtitle =
      activity_prefs_sleep_insights_are_enabled() ? "Enabled" : "Disabled";
  data->debug_card.menu_items[3].subtitle = dls_get_send_enable() ? "Enabled" : "Disabled";
}


// -------------------------------------------------------------------------------
static void debug_window_unload(Window *window) {
  ActivityDemoAppData *data = window_get_user_data(window);
  simple_menu_layer_destroy(data->debug_card.menu_layer);
}


// -------------------------------------------------------------------------------
static void deinit(void) {
  ActivityDemoAppData *data = app_state_get_user_data();
  window_destroy(data->debug_window);
  app_free(data);
}


// -------------------------------------------------------------------------------
static void init(void) {
  ActivityDemoAppData *data = app_malloc_check(sizeof(ActivityDemoAppData));
  s_data = data;
  memset(data, 0, sizeof(ActivityDemoAppData));
  app_state_set_user_data(data);

  // Debug window
  data->debug_window = window_create();
  window_set_user_data(data->debug_window, data);
  window_set_window_handlers(data->debug_window, &(WindowHandlers) {
    .load = debug_window_load,
    .unload = debug_window_unload,
  });

  app_window_stack_push(data->debug_window, true /* Animated */);
}


// -------------------------------------------------------------------------------
static void s_main(void) {
  init();
  app_event_loop();
  deinit();
}


// -------------------------------------------------------------------------------
const PebbleProcessMd* activity_demo_get_app_info(void) {
  static const PebbleProcessMdSystem s_activity_demo_app_info = {
    .common.main_func = &s_main,
    // UUID: 60206d97-818b-4f42-87ae-48fde623608d
    .common.uuid = {0x60, 0x20, 0x6d, 0x97, 0x81, 0x8b, 0x4f, 0x42, 0x87, 0xae, 0x48, 0xfd, 0xe6,
                    0x23, 0x60, 0x8d},
    .name = "ActivityDemo"
  };
  return (const PebbleProcessMd*) &s_activity_demo_app_info;
}
