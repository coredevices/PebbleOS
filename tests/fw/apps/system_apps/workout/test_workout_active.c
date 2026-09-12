/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system/workout/workout.h"
#include "apps/system/workout/active.h"
#include "apps/system/workout/hr_history.h"
#include "apps/system/workout/data.h"
#include "apps/system/workout/dialog.h"

#include "pbl/services/activity/health_util.h"

#define shell_prefs_get_units_distance prv_default_distance_units
#include "test_workout_app_includes.h"
#undef shell_prefs_get_units_distance

#include "stubs_window_manager.h"

bool s_hrm_is_present;
static UnitsDistance s_distance_units;

UnitsDistance shell_prefs_get_units_distance(void) {
  return s_distance_units;
}

// Fakes
/////////////////////
extern void prv_cycle_scrollable_metrics(WorkoutActiveWindow *active_window);
extern bool prv_finish_workout(WorkoutActiveWindow *active_window);
extern void prv_update_timer_callback(void *data);

bool activity_is_hrm_present(void) {
  return s_hrm_is_present;
}

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  return 0;
}

void workout_push_summary_window(void) {
  return;
}

static WorkoutData s_workout_data;

static WorkoutController s_workout_controller = {
    .is_paused = workout_service_is_paused,
    .pause = workout_service_pause_workout,
    .stop = workout_service_stop_workout,
    .update_data = workout_data_update,
    .metric_to_string = workout_data_fill_metric_value,
    .get_metric_value = workout_data_get_metric_value,
    .get_distance_string = health_util_get_distance_string,
};

typedef struct SportsData {
  int32_t current_bpm;
  char *duration_string;
  char *distance_string;
  char *pace_string;
  char *custom_label_string;
  char *custom_value_string;
} SportsData;

static SportsData s_sports_data;

static bool prv_is_sports_paused(void) {
  return false;
}
static bool prv_sports_pause(bool should_be_paused) {
  return false;
}
static void prv_metric_to_string(WorkoutMetricType type, char *buffer, size_t buffer_size,
                                 void *i18n_owner, void *sports_data) {
  SportsData *data = sports_data;

  switch (type) {
    case WorkoutMetricType_Hr: {
      snprintf(buffer, buffer_size, "%d", data->current_bpm);
      break;
    }
    case WorkoutMetricType_Speed:
    case WorkoutMetricType_Pace: {
      strncpy(buffer, data->pace_string, buffer_size);
      break;
    }
    case WorkoutMetricType_Distance: {
      strncpy(buffer, data->distance_string, buffer_size);
      break;
    }
    case WorkoutMetricType_Duration: {
      strncpy(buffer, data->duration_string, buffer_size);
      break;
    }
    case WorkoutMetricType_Custom: {
      strncpy(buffer, data->custom_value_string, buffer_size);
      break;
    }
    // Not supported by the sports API
    case WorkoutMetricType_Steps:
    case WorkoutMetricType_AvgPace:
    case WorkoutMetricType_AvgSpeed:
    case WorkoutMetricType_AvgCadence:
    case WorkoutMetricType_ActiveCalories:
    case WorkoutMetricType_AvgHr:
    case WorkoutMetricType_None:
    case WorkoutMetricTypeCount:
      break;
  }
}

static int32_t prv_sports_get_value(WorkoutMetricType type, void *sports_data) {
  SportsData *data = sports_data;
  switch (type) {
    case WorkoutMetricType_Hr:
      return data->current_bpm;
    default:
      return 0;
  }
}

static char *prv_get_custom_metric_label_string(void) {
  return s_sports_data.custom_label_string;
}

static WorkoutController s_sports_controller = {
    .is_paused = prv_is_sports_paused,
    .pause = prv_sports_pause,
    .stop = NULL,
    .update_data = NULL,
    .metric_to_string = prv_metric_to_string,
    .get_metric_value = prv_sports_get_value,
    .get_distance_string = health_util_get_distance_string,
    .get_custom_metric_label_string = prv_get_custom_metric_label_string,
};

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_workout_active__initialize(void) {
  workout_service_set_active_kcalories(0);
  workout_service_set_avg_hr(0);
  s_distance_units = UnitsDistance_Miles;
  s_hrm_is_present = true;
  workout_service_pause_workout(false);
  workout_service_stop_workout();

  s_workout_data = (WorkoutData){};
  s_sports_data = (SportsData){};

  // Setup graphics context
  framebuffer_init(&s_fb, &(GSize){DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;

  // Setup resources
  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();

  // Setup content indicator
  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}

void test_workout_active__cleanup(void) {}

// Helpers
//////////////////////

static void prv_create_window_and_render(WorkoutActiveWindow *active_window,
                                         int secondary_metric_idx) {
  for (int i = 0; i < secondary_metric_idx; i++) {
    prv_cycle_scrollable_metrics(active_window);
  }

  Window *window = (Window *)active_window;
  window_set_on_screen(window, true, true);
  window_render(window, &s_ctx);
}

// Workout Tests
//////////////////////

void test_workout_active__workout_render_no_data(void) {
  s_workout_data = (WorkoutData){};
  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_walk(void) {
  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 84,
      .distance_m = 1234,
      .avg_pace = health_util_get_pace(84, 1234),
      .bpm = 71,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Walk, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_walk_no_hrm(void) {
  s_hrm_is_present = false;

  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 84,
      .distance_m = 1234,
      .avg_pace = health_util_get_pace(84, 1234),
      .bpm = 71,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Walk, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_run(void) {
  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 84,
      .distance_m = 1234,
      .avg_pace = health_util_get_pace(84, 1234),
      .bpm = 71,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_run_no_hrm(void) {
  s_hrm_is_present = false;

  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 84,
      .distance_m = 1234,
      .avg_pace = health_util_get_pace(84, 1234),
      .bpm = 71,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_open_workout(void) {
  s_workout_data = (WorkoutData){
      .steps = 0,
      .duration_s = 84,
      .distance_m = 0,
      .avg_pace = health_util_get_pace(84, 0),
      .bpm = 92,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Open, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_open_workout_no_hrm(void) {
  s_hrm_is_present = false;

  s_workout_data = (WorkoutData){
      .steps = 0,
      .duration_s = 84,
      .distance_m = 0,
      .avg_pace = health_util_get_pace(84, 0),
      .bpm = 92,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Open, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_hr_zone_1(void) {
  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 789,
      .distance_m = 234,
      .avg_pace = health_util_get_pace(789, 234),
      .bpm = 148,
      .hr_zone = 1,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_hr_zone_2(void) {
  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 789,
      .distance_m = 234,
      .avg_pace = health_util_get_pace(789, 234),
      .bpm = 167,
      .hr_zone = 2,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_hr_zone_3(void) {
  s_workout_data = (WorkoutData){
      .steps = 567,
      .duration_s = 789,
      .distance_m = 234,
      .avg_pace = health_util_get_pace(789, 234),
      .bpm = 197,
      .hr_zone = 3,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_very_slow_pace(void) {
  s_workout_data = (WorkoutData){
      .steps = 0,
      .duration_s = SECONDS_PER_HOUR,
      .distance_m = 1609,
      .avg_pace = health_util_get_pace(SECONDS_PER_HOUR, 1609),
      .bpm = 0,
      .hr_zone = 0,
  };

  WorkoutActiveWindow *active_window = workout_active_create_for_activity_type(
      ActivitySessionType_Walk, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

// Workout Tests
//////////////////////

void test_workout_active__sports_pace(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_pace_long_values(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "04:20:39",
      .distance_string = "115.12",
      .pace_string = "12:34",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_speed(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "20:00",
      .distance_string = "18.9",
      .pace_string = "35.3",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Speed, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_no_hrm(void) {
  s_hrm_is_present = false;

  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_hr_z0(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_hr_z1(void) {
  s_sports_data = (SportsData){
      .current_bpm = 135,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_hr_z2(void) {
  s_sports_data = (SportsData){
      .current_bpm = 165,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_hr_z3(void) {
  s_sports_data = (SportsData){
      .current_bpm = 180,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Hr};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_custom_field(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
      .custom_label_string = "CUSTOM",
      .custom_value_string = "000000",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Custom};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_custom_long_values(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
      .custom_label_string = "CUSTOM FIELD LABEL",
      .custom_value_string = "0000000000000000000",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Custom};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__sports_custom_hanging_label(void) {
  s_sports_data = (SportsData){
      .current_bpm = 71,
      .duration_string = "30:00",
      .distance_string = "5.0",
      .pace_string = "6:00",
      .custom_label_string = "Hanging Field",
      .custom_value_string = "000000",
  };

  WorkoutMetricType top_metric = WorkoutMetricType_Duration;
  WorkoutMetricType middle_metric = WorkoutMetricType_Distance;
  WorkoutMetricType scrollable_metrics[] = {WorkoutMetricType_Pace, WorkoutMetricType_Custom};

  WorkoutActiveWindow *active_window = workout_active_create_triple_layout(
      top_metric, middle_metric, ARRAY_LENGTH(scrollable_metrics), scrollable_metrics,
      &s_sports_data, &s_sports_controller);
  prv_create_window_and_render(active_window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_paused(void) {
  s_workout_data = (WorkoutData){.duration_s = 754, .distance_m = 2400, .avg_pace = 314};
  workout_service_pause_workout(true);
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_completed(void) {
  workout_service_start_workout(ActivitySessionType_Run);
  workout_service_set_current_workout_info(3210, 754, 2400, 148, HRZone_Zone2);
  workout_service_set_active_kcalories(125);
  workout_service_set_avg_hr(142);
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  cl_assert(prv_finish_workout(window));
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_i(s_workout_data.duration_s, 754);
  cl_assert_equal_i(s_workout_data.distance_m, 2400);
  cl_assert_equal_i(s_workout_data.active_kcal, 125);
  cl_assert_equal_i(s_workout_data.avg_bpm, 142);
  workout_service_set_current_workout_info(0, 0, 0, 0, HRZone_Zone0);
  workout_service_set_active_kcalories(0);
  workout_service_set_avg_hr(0);
  prv_update_timer_callback(window);
  cl_assert_equal_i(s_workout_data.active_kcal, 125);
  cl_assert_equal_i(s_workout_data.avg_bpm, 142);
  cl_assert_equal_i(s_workout_data.duration_s, 754);
  cl_assert_equal_i(s_workout_data.distance_m, 2400);
  prv_create_window_and_render(window, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_long_values(void) {
  s_workout_data = (WorkoutData){
      .duration_s = 45296, .distance_m = 123456, .steps = 123456, .avg_pace = 367, .bpm = 148};
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__new_workout_clears_previous_pace(void) {
  s_workout_data.avg_pace = 300;
  workout_service_set_current_workout_info(0, 0, 0, 0, HRZone_Zone0);
  workout_data_update(&s_workout_data);
  cl_assert_equal_i(s_workout_data.avg_pace, 0);
}

void test_workout_active__single_metric_cycle_is_safe(void) {
  WorkoutActiveWindow *window = workout_active_create_single_layout(
      WorkoutMetricType_Duration, &s_workout_data, &s_workout_controller);
  prv_cycle_scrollable_metrics(window);
}

void test_workout_active__derived_metrics_and_units(void) {
  WorkoutData data = {.duration_s = 1800, .distance_m = 5000, .steps = 4500};
  cl_assert_equal_i(10000, workout_data_get_metric_value(WorkoutMetricType_AvgSpeed, &data));
  cl_assert_equal_i(150, workout_data_get_metric_value(WorkoutMetricType_AvgCadence, &data));
  char text[24];
  s_distance_units = UnitsDistance_KM;
  workout_data_fill_metric_value(WorkoutMetricType_AvgSpeed, text, sizeof(text), &data, &data);
  cl_assert_equal_s("10.0", text);
  s_distance_units = UnitsDistance_Miles;
  workout_data_fill_metric_value(WorkoutMetricType_AvgSpeed, text, sizeof(text), &data, &data);
  cl_assert_equal_s("6.2", text);
  data.duration_s = 0;
  cl_assert_equal_i(-1, workout_data_get_metric_value(WorkoutMetricType_AvgSpeed, &data));
  cl_assert_equal_i(-1, workout_data_get_metric_value(WorkoutMetricType_AvgCadence, &data));
  data = (WorkoutData){.duration_s = 60};
  cl_assert_equal_i(0, workout_data_get_metric_value(WorkoutMetricType_AvgSpeed, &data));
  cl_assert_equal_i(0, workout_data_get_metric_value(WorkoutMetricType_AvgCadence, &data));
  data = (WorkoutData){.duration_s = 3600, .distance_m = 1000000, .steps = 180000};
  cl_assert_equal_i(1000000, workout_data_get_metric_value(WorkoutMetricType_AvgSpeed, &data));
  cl_assert_equal_i(3000, workout_data_get_metric_value(WorkoutMetricType_AvgCadence, &data));
}

void test_workout_active__workout_render_cadence(void) {
  s_workout_data = (WorkoutData){
      .duration_s = 1800, .distance_m = 5000, .steps = 4500, .active_kcal = 320, .bpm = 148};
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(window, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_speed_and_calories(void) {
  s_distance_units = UnitsDistance_KM;
  s_workout_data = (WorkoutData){.duration_s = 1800,
                                 .distance_m = 5000,
                                 .steps = 4500,
                                 .active_kcal = 320,
                                 .bpm = 148,
                                 .avg_pace = 360};
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(window, 2);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_heart_page(void) {
  s_workout_data = (WorkoutData){
      .duration_s = 1800, .distance_m = 5000, .steps = 4500, .bpm = 148, .avg_bpm = 142};
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_create_window_and_render(window, 3);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

extern void prv_select_click_handler(ClickRecognizerRef recognizer, void *context);
extern void prv_down_click_handler(ClickRecognizerRef recognizer, void *context);

void test_workout_active__button_pause_cancel_resume_and_finish(void) {
  workout_service_start_workout(ActivitySessionType_Run);
  workout_service_set_current_workout_info(4500, 1800, 5000, 148, HRZone_Zone1);
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_select_click_handler(NULL, window);
  cl_assert(workout_service_is_paused());
  prv_down_click_handler(NULL, window);
  cl_assert(workout_service_is_workout_ongoing());
  prv_down_click_handler(NULL, window);
  prv_select_click_handler(NULL, window);
  cl_assert(!workout_service_is_paused());
  prv_select_click_handler(NULL, window);
  prv_down_click_handler(NULL, window);
  prv_select_click_handler(NULL, window);
  cl_assert(!workout_service_is_workout_ongoing());
  cl_assert_equal_i(s_workout_data.duration_s, 1800);
}

extern void prv_record_hr_sample(WorkoutActiveWindow *window);
extern void prv_set_hr_plot(WorkoutActiveWindow *window, bool visible, bool animated);

void test_workout_active__hr_history_rolls_after_one_minute(void) {
  WorkoutHrHistory history = {};
  for (int second = 0; second < 120; second++) {
    workout_hr_history_add(&history, second, 100 + second);
  }
  cl_assert_equal_i(history.count, 60);
  for (int i = 0; i < 60; i++) {
    const WorkoutHrSample *sample = workout_hr_history_get(&history, i);
    cl_assert_equal_i(sample->elapsed_s, i + 60);
    cl_assert_equal_i(sample->bpm, i + 160);
  }
  cl_assert_equal_p(workout_hr_history_get(&history, 60), NULL);
}

void test_workout_active__hr_history_duplicate_missing_and_reset(void) {
  WorkoutHrHistory history = {};
  workout_hr_history_add(&history, 10, 140);
  workout_hr_history_add(&history, 10, 142);
  cl_assert_equal_i(history.count, 1);
  cl_assert_equal_i(workout_hr_history_get(&history, 0)->bpm, 142);
  workout_hr_history_add(&history, 11, 0);
  workout_hr_history_add(&history, 15, 155);
  cl_assert_equal_i(history.count, 3);
  cl_assert_equal_i(workout_hr_history_get(&history, 1)->bpm, 0);
  cl_assert_equal_i(workout_hr_history_get(&history, 2)->elapsed_s, 15);
  workout_hr_history_add(&history, 0, 130);
  cl_assert_equal_i(history.count, 1);
  cl_assert_equal_i(workout_hr_history_get(&history, 0)->elapsed_s, 0);
}

void test_workout_active__hr_history_autoscale(void) {
  WorkoutHrHistory history = {0};
  int minimum;
  int maximum;
  cl_assert(!workout_hr_history_range(&history, 0, &minimum, &maximum));
  workout_hr_history_add(&history, 0, 200);
  workout_hr_history_add(&history, 1, 132);
  workout_hr_history_add(&history, 59, 157);
  workout_hr_history_add(&history, 60, 0);
  workout_hr_history_add(&history, 61, 190);
  cl_assert(workout_hr_history_range(&history, 60, &minimum, &maximum));
  cl_assert_equal_i(minimum, 130);
  cl_assert_equal_i(maximum, 160);
  cl_assert(!workout_hr_history_range(&history, 121, &minimum, &maximum));
  workout_hr_history_add(&history, 122, 140);
  cl_assert(workout_hr_history_range(&history, 122, &minimum, &maximum));
  cl_assert_equal_i(minimum, 130);
  cl_assert_equal_i(maximum, 150);
}

static void prv_render_history(WorkoutActiveWindow *window) {
  for (int i = 0; i < 3; i++) {
    prv_cycle_scrollable_metrics(window);
  }
  prv_set_hr_plot(window, true, false);
  prv_create_window_and_render(window, 0);
}

void test_workout_active__workout_render_heart_plot(void) {
  s_workout_data = (WorkoutData){.avg_bpm = 145};
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  for (int second = 1; second <= 90; second++) {
    s_workout_data.duration_s = second;
    s_workout_data.bpm = second >= 65 && second <= 70 ? 0 : 120 + second % 60;
    prv_record_hr_sample(window);
  }
  prv_render_history(window);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_active__workout_render_heart_plot_no_data(void) {
  s_workout_data = (WorkoutData){.duration_s = 60};
  WorkoutActiveWindow *window = workout_active_create_for_activity_type(
      ActivitySessionType_Run, &s_workout_data, &s_workout_controller);
  prv_render_history(window);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
