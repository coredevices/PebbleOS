/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system/timeline/pin_window.h"
#include "pbl/services/timeline/sports_layout.h"
#include "pbl/services/timeline/weather_layout.h"

#include "clar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fakes
/////////////////////

#include "fake_content_indicator.h"
#include "fixtures/load_test_resources.h"

bool property_animation_init(PropertyAnimation *animation,
                             const PropertyAnimationImplementation *implementation, void *subject,
                             void *from_value, void *to_value) {
  if (!animation) {
    return false;
  }

  PropertyAnimationPrivate *animation_private = (PropertyAnimationPrivate *)animation;
  *animation_private = (PropertyAnimationPrivate){
    .animation.implementation = (const AnimationImplementation *)implementation,
    .subject = subject,
  };

  if (from_value) {
    animation_private->values.from.int16 = *(int16_t *)from_value;
  }

  if (to_value) {
    animation_private->values.to.int16 = *(int16_t *)to_value;
  }

  return true;
}

void clock_get_friendly_date(char *buffer, int buf_size, time_t timestamp) {
  if (buffer) {
    strncpy(buffer, "Today", buf_size);
    buffer[buf_size - 1] = '\0';
  }
}

void clock_get_since_time(char *buffer, int buf_size, time_t timestamp) {
  if (buffer) {
    strncpy(buffer, "15 minutes ago", buf_size);
    buffer[buf_size - 1] = '\0';
  }
}

void clock_get_until_time_capitalized(char *buffer, int buf_size, time_t timestamp,
                                      int max_relative_hrs) {
  if (buffer) {
    strncpy(buffer, "IN 2 HOURS", buf_size);
    buffer[buf_size - 1] = '\0';
  }
}

// Stubs
/////////////////////

#include "stubs_action_menu.h"
#include "stubs_alerts_preferences.h"
#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_timer.h"
#include "stubs_app_window_stack.h"
#include "stubs_bootbits.h"
#include "stubs_click.h"
#include "stubs_event_service_client.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_info.h"
#include "stubs_pebble_tasks.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_property_animation.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_syscalls.h"
#include "stubs_task_wdt.h"
#include "stubs_timeline.h"
#include "stubs_timeline_actions.h"
#include "stubs_timeline_item.h"
#include "stubs_timeline_layer.h"
#include "stubs_timeline_peek.h"
#include "stubs_vibes.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Helper Functions
/////////////////////

#include "fw/graphics/util.h"

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer *fb = NULL;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_timeline_layouts__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize){DISP_COLS, DISP_ROWS});

  const GContextInitializationMode context_init_mode = GContextInitializationMode_System;
  graphics_context_init(&s_ctx, fb, context_init_mode);

  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);

  resource_init();

  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}

void test_timeline_layouts__cleanup(void) {
  free(fb);
}

// Helpers
//////////////////////

void prv_handle_down_click(ClickRecognizerRef recognizer, void *context);

static void prv_render_layout(LayoutId layout_id, const AttributeList *attr_list,
                              size_t num_down_clicks) {
  PBL_ASSERTN(attr_list);

  TimelineItem item = (TimelineItem){
    .header =
        (CommonTimelineItemHeader){
          .layout = layout_id,
          .type = TimelineItemTypePin,
        },
    .attr_list = *attr_list,
  };

  TimelinePinWindow pin_window = (TimelinePinWindow){};
  timeline_pin_window_init(&pin_window, &item, rtc_get_time());
  Window *window = &pin_window.window;

  window_set_on_screen(window, true, true);

  for (int i = 0; i < num_down_clicks; i++) {
    prv_handle_down_click(NULL, &pin_window.item_detail_layer);

    // Aint nobody got time for animations; advance the scrolling property animation to completion
    int16_t to = 0;
    if (property_animation_get_to_int16(pin_window.item_detail_layer.animation, &to)) {
      pin_window.item_detail_layer.scroll_offset_pixels = to;
    }
  }

  window_render(window, &s_ctx);
}

typedef struct TimelineLayoutTestConfig {
  LayoutId layout_id;
  const char *title;
  const char *subtitle;
  const char *location_name;
  const char *body;
  TimelineResourceId icon_timeline_res_id;
  WeatherTimeType weather_time_type;
  uint8_t weather_pin_kind;
} TimelineLayoutTestConfig;

static void prv_construct_and_render_layout(const TimelineLayoutTestConfig *config,
                                            size_t num_down_clicks) {
  if (!config) {
    return;
  }

  AttributeList attr_list = (AttributeList){0};
  if (config->title) {
    attribute_list_add_cstring(&attr_list, AttributeIdTitle, config->title);
  }
  if (config->subtitle) {
    attribute_list_add_cstring(&attr_list, AttributeIdSubtitle, config->subtitle);
  }
  if (config->location_name) {
    attribute_list_add_cstring(&attr_list, AttributeIdLocationName, config->location_name);
  }
  if (config->body) {
    attribute_list_add_cstring(&attr_list, AttributeIdBody, config->body);
  }
  if (config->icon_timeline_res_id != TIMELINE_RESOURCE_INVALID) {
    attribute_list_add_resource_id(&attr_list, AttributeIdIconPin, config->icon_timeline_res_id);
  }
  attribute_list_add_uint8(&attr_list, AttributeIdDisplayTime, config->weather_time_type);
  if (config->weather_pin_kind) {
    attribute_list_add_uint8(&attr_list, AttributeIdWeatherPinKind, config->weather_pin_kind);
  }
  // Just need to put something here so our mocked clock_get_since_time() gets called
  attribute_list_add_uint32(&attr_list, AttributeIdLastUpdated, 1337);

  prv_render_layout(config->layout_id, &attr_list, num_down_clicks);

  attribute_list_destroy_list(&attr_list);
}

// Tests
//////////////////////

void test_timeline_layouts__generic(void) {
  const TimelineLayoutTestConfig config = (TimelineLayoutTestConfig){
    .layout_id = LayoutIdGeneric,
    .title = "Delfina Pizza",
    .subtitle = "Open Table Reservation",
    .location_name = "145 Williams\nJohn Ave, Palo Alto",
    .body = "Body message",
    .icon_timeline_res_id = TIMELINE_RESOURCE_DINNER_RESERVATION,
  };

  prv_construct_and_render_layout(&config, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(peek)));

  prv_construct_and_render_layout(&config, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(details1)));

  // Round only needs to scroll down once to see everything
#if !PBL_ROUND
  prv_construct_and_render_layout(&config, 2);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(details2)));
#endif
}

void test_timeline_layouts__weather(void) {
  const TimelineLayoutTestConfig config = (TimelineLayoutTestConfig){
    .layout_id = LayoutIdWeather,
    .title = "The Greatest Sunrise Ever",
    .subtitle = "90°/60°",
    .location_name = "Redwood City",
    .body = "A clear sky. Low around 60F.",
    .icon_timeline_res_id = TIMELINE_RESOURCE_PARTLY_CLOUDY,
    .weather_time_type = WeatherTimeType_Pin,
  };

  prv_construct_and_render_layout(&config, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(peek)));

  prv_construct_and_render_layout(&config, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(details1)));

  // Round needs to scroll down one more time to see everything
#if PBL_ROUND
  prv_construct_and_render_layout(&config, 2);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(details2)));
#endif
}

static void prv_check_renders_like(const TimelineLayoutTestConfig *config,
                                   const TimelineLayoutTestConfig *reference,
                                   size_t num_down_clicks) {
  const size_t size = s_ctx.dest_bitmap.row_size_bytes * s_ctx.dest_bitmap.bounds.size.h;
  uint8_t *expected = malloc(size);
  prv_construct_and_render_layout(reference, num_down_clicks);
  memcpy(expected, s_ctx.dest_bitmap.addr, size);
  prv_construct_and_render_layout(config, num_down_clicks);
  cl_assert(memcmp(expected, s_ctx.dest_bitmap.addr, size) == 0);
  free(expected);
}

void test_timeline_layouts__weather_pin_kind(void) {
  const TimelineLayoutTestConfig reference = (TimelineLayoutTestConfig){
    .layout_id = LayoutIdWeather,
    .title = "Sunset",
    .subtitle = "90°/60°",
    .location_name = "Redwood City",
    .body = "A clear sky. Low around 60F.",
    .icon_timeline_res_id = TIMELINE_RESOURCE_PARTLY_CLOUDY,
    .weather_time_type = WeatherTimeType_Pin,
  };

  TimelineLayoutTestConfig with_kind = reference;
  with_kind.title = "Ignored title";
  with_kind.weather_pin_kind = WeatherPinKind_Sunset;
  prv_check_renders_like(&with_kind, &reference, 0);
  prv_check_renders_like(&with_kind, &reference, 1);

  TimelineLayoutTestConfig unknown_kind = reference;
  unknown_kind.weather_pin_kind = 0xFF;
  prv_check_renders_like(&unknown_kind, &reference, 0);
  prv_check_renders_like(&unknown_kind, &reference, 1);
}

static void prv_construct_and_render_sports_layout(GameState state, size_t num_down_clicks) {
  AttributeList attr_list = (AttributeList){0};
  attribute_list_add_cstring(&attr_list, AttributeIdTitle, "Warriors at Bulls");
  attribute_list_add_uint8(&attr_list, AttributeIdSportsGameState, state);
  attribute_list_add_cstring(&attr_list, AttributeIdNameAway, "GSW");
  attribute_list_add_cstring(&attr_list, AttributeIdNameHome, "CHI");
  if (state == GameStatePreGame) {
    attribute_list_add_cstring(&attr_list, AttributeIdRecordAway, "42-18");
    attribute_list_add_cstring(&attr_list, AttributeIdRecordHome, "35-25");
    attribute_list_add_cstring(&attr_list, AttributeIdBody, "United Center, Chicago");
  } else {
    attribute_list_add_cstring(&attr_list, AttributeIdSubtitle, "Q3 - 4:12");
    attribute_list_add_cstring(&attr_list, AttributeIdScoreAway, "78");
    attribute_list_add_cstring(&attr_list, AttributeIdScoreHome, "81");
    attribute_list_add_cstring(&attr_list, AttributeIdBody, "Curry 3pt Shot: Made");
  }
  attribute_list_add_cstring(&attr_list, AttributeIdBroadcaster, "ESPN");
  attribute_list_add_uint32(&attr_list, AttributeIdLastUpdated, 1337);

  prv_render_layout(LayoutIdSports, &attr_list, num_down_clicks);

  attribute_list_destroy_list(&attr_list);
}

void test_timeline_layouts__sports_pregame(void) {
  prv_construct_and_render_sports_layout(GameStatePreGame, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(peek)));

  prv_construct_and_render_sports_layout(GameStatePreGame, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(details1)));
}

void test_timeline_layouts__sports_ingame(void) {
  prv_construct_and_render_sports_layout(GameStateInGame, 0);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(peek)));

  prv_construct_and_render_sports_layout(GameStateInGame, 1);
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE_X(details1)));
}
