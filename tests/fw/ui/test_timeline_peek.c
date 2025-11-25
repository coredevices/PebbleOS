/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/bitblt.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window_private.h"
#include "popups/timeline/peek_private.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"
#include "util/buffer.h"
#include "util/graphics.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/size.h"
#include "util/trig.h"

#include "clar.h"

#include <stdio.h>

// Fakes
/////////////////////

#include "fake_rtc.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

void clock_get_until_time(char *buffer, int buf_size, time_t timestamp, int max_relative_hrs) {
  snprintf(buffer, buf_size, "In 5 minutes");
}

// Stubs
/////////////////////

#include "stubs_activity.h"
#include "stubs_analytics.h"
#include "stubs_animation_timing.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_bootbits.h"
#include "stubs_click.h"
#include "stubs_cron.h"
#include "stubs_event_loop.h"
#include "stubs_i18n.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_menu_cell_layer.h"
#include "stubs_modal_manager.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_process_info.h"
#include "stubs_pebble_tasks.h"
#include "stubs_pin_db.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_property_animation.h"
#include "stubs_scroll_layer.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_timeline_event.h"
#include "stubs_timeline_layer.h"
#include "stubs_unobstructed_area.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

// Helper Functions
/////////////////////

#include "fw/graphics/test_graphics.h"
#include "fw/graphics/util.h"

static GContext s_ctx;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

static bool s_is_watchface_running;

bool app_manager_is_watchface_running(void) {
  return s_is_watchface_running;
}

// Setup and Teardown
////////////////////////////////////

static FrameBuffer s_fb;

static GBitmap *s_dest_bitmap;

void test_timeline_peek__initialize(void) {
  // Setup time
  TimezoneInfo tz_info = {
    .tm_zone = "UTC",
  };
  time_util_update_timezone(&tz_info);
  rtc_set_timezone(&tz_info);
  rtc_set_time(SECONDS_PER_DAY);

  // We start time out at 5pm on Jan 1, 2015 for all of these tests
  struct tm time_tm = {
    // Thursday, Jan 1, 2015, 5pm
    .tm_hour = 17,
    .tm_mday = 1,
    .tm_year = 115
  };

  const time_t utc_sec = mktime(&time_tm);
  fake_rtc_init(0 /* initial_ticks */, utc_sec);

  // Setup graphics context
  framebuffer_init(&s_fb, &DISP_FRAME.size);
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

  // Initialize peek
  s_is_watchface_running = true;
  timeline_peek_init();
}

void test_timeline_peek__cleanup(void) {
}

// Helpers
//////////////////////

static void prv_render_layer(Layer *layer, const GRect *box, bool use_screen) {
  gbitmap_destroy(s_dest_bitmap);

  const GRect *drawing_box = use_screen ? &DISP_FRAME : box;
  const GSize bitmap_size = drawing_box->size;
  s_dest_bitmap = gbitmap_create_blank(bitmap_size, GBITMAP_NATIVE_FORMAT);

  s_ctx.dest_bitmap = *s_dest_bitmap;
  s_ctx.draw_state.clip_box.size = bitmap_size;
  s_ctx.draw_state.drawing_box = *drawing_box;

  layer_render_tree(layer, &s_ctx);

  if (use_screen) {
    GBitmap *screen_bitmap = s_dest_bitmap;
    screen_bitmap->bounds = (GRect) { gpoint_neg(box->origin), box->size };
    s_dest_bitmap = gbitmap_create_blank(box->size, PBL_IF_COLOR_ELSE(GBitmapFormat8Bit, GBitmapFormat1Bit));
    bitblt_bitmap_into_bitmap(s_dest_bitmap, screen_bitmap, GPointZero, GCompOpAssign,
                              GColorClear);
    gbitmap_destroy(screen_bitmap);
  }
}

typedef struct TimelinePeekItemConfig {
  time_t timestamp;
  const char *title;
  const char *subtitle;
  TimelineResourceId icon;
  int num_concurrent;
} TimelinePeekItemConfig;

static TimelineItem *prv_set_timeline_item(const TimelinePeekItemConfig *config, bool animated) {
  TimelineItem *item = NULL;
  const time_t now = rtc_get_time();
  const time_t timestamp = config ? (config->timestamp ?: now) : now;
  if (config) {
    AttributeList list;
    attribute_list_init_list(3 /* num_attributes */, &list);
    attribute_list_add_cstring(&list, AttributeIdTitle, config->title);
    if (config->subtitle) {
      attribute_list_add_cstring(&list, AttributeIdSubtitle, config->subtitle);
    }
    attribute_list_add_uint32(&list, AttributeIdIconPin, config->icon);
    item = timeline_item_create_with_attributes(timestamp, MINUTES_PER_HOUR,
                                                TimelineItemTypePin, LayoutIdGeneric,
                                                &list, NULL);
    attribute_list_destroy_list(&list);
  }
  timeline_peek_set_item(item, timestamp >= now, config ? config->num_concurrent : 0,
                         false /* first */, animated);
  return item;
}

static void prv_render_timeline_peek(const TimelinePeekItemConfig *config) {
  TimelineItem *item = prv_set_timeline_item(config, false /* animated */);
  // Force timeline peek to be visible
  timeline_peek_set_visible(true, false /* animated */);

  TimelinePeek *peek = timeline_peek_get_peek();
  const Layer *layer = &peek->layout_layer;
  // For text flow, the whole screen is needed. Render the screen, then reduce to the layer.
  const bool use_screen = PBL_IF_ROUND_ELSE(true, false);
  prv_render_layer(&peek->window.layer,
                   &(GRect) { gpoint_neg(layer->frame.origin), layer->frame.size },
                   use_screen);

  timeline_item_destroy(item);
}

// Visual Layout Tests
//////////////////////

void test_timeline_peek__peek(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "CoreUX Design x Eng",
    .subtitle = "ConfRM-Missile Command",
    .icon = TIMELINE_RESOURCE_TIMELINE_CALENDAR,
    .num_concurrent = 0,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_newline(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "NY 3\nSF 12",
    .subtitle = "Bottom of\nthe 9th",
    .icon = TIMELINE_RESOURCE_TIMELINE_BASEBALL,
    .num_concurrent = 1,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_title_only_newline(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "NY 3\nSF 12",
    .icon = TIMELINE_RESOURCE_TIMELINE_BASEBALL,
    .num_concurrent = 1,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_concurrent_1(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "NY 3 - SF 12",
    .subtitle = "Bottom of the 9th",
    .icon = TIMELINE_RESOURCE_TIMELINE_BASEBALL,
    .num_concurrent = 1,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_concurrent_2(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "Stock for party 🍺",
    .subtitle = "Pebble Pad on Park",
    .icon = TIMELINE_RESOURCE_NOTIFICATION_REMINDER,
    .num_concurrent = 2,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_concurrent_2_max(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = ":parrot: :parrot:",
    .subtitle = ":parrot: :parrot: :parrot:",
    .icon = TIMELINE_RESOURCE_GENERIC_CONFIRMATION,
    .num_concurrent = 3,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_title_only(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "Trash up the Place 🔥",
    .icon = TIMELINE_RESOURCE_TIDE_IS_HIGH,
    .num_concurrent = 0,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_title_only_concurrent_1(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "No Watch No Life",
    .icon = TIMELINE_RESOURCE_DAY_SEPARATOR,
    .num_concurrent = 1,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_title_only_concurrent_2(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .title = "OMG I think the text fits!",
    .icon = TIMELINE_RESOURCE_GENERIC_WARNING,
    .num_concurrent = 2,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_timeline_peek__peek_in_5_minutes(void) {
  prv_render_timeline_peek(&(TimelinePeekItemConfig) {
    .timestamp = rtc_get_time() + (5 * SECONDS_PER_MINUTE),
    .title = "Stock for party 🍺",
    .subtitle = "Pebble Pad on Park",
    .icon = TIMELINE_RESOURCE_NOTIFICATION_REMINDER,
    .num_concurrent = 2,
  });
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

// Visibility Tests
//////////////////////

void test_timeline_peek__peek_visibility(void) {
  prv_set_timeline_item(NULL, false /* animated */);
  TimelinePeek *peek = timeline_peek_get_peek();
  const Layer *layer = &peek->layout_layer;
  // Normally it is animated, but for this unit test, we don't request `animated`
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Peek service shows the peek UI. Not animated for this unit test.
  TimelineItem *item = prv_set_timeline_item(&(TimelinePeekItemConfig) {
    .title = "CoreUX Design x Eng",
    .subtitle = "ConfRM-Missile Command",
    .icon = TIMELINE_RESOURCE_TIMELINE_CALENDAR,
    .num_concurrent = 0,
  }, false /* animated */);
  // Peek should now be on-screen.
  cl_assert(layer->frame.origin.y < DISP_ROWS);

  // Peek service hides the peek UI. Not animated for this unit test.
  prv_set_timeline_item(NULL, false /* animated */);
  // Peek should now be off-screen.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);
}

void test_timeline_peek__peek_visible_to_hidden_outside_of_watchface(void) {
  TimelineItem *item = prv_set_timeline_item(&(TimelinePeekItemConfig) {
    .title = "CoreUX Design x Eng",
    .subtitle = "ConfRM-Missile Command",
    .icon = TIMELINE_RESOURCE_TIMELINE_CALENDAR,
    .num_concurrent = 0,
  }, false /* animated */);
  TimelinePeek *peek = timeline_peek_get_peek();
  const Layer *layer = &peek->layout_layer;
  // Normally it is animated, but for this unit test, we don't request `animated`
  cl_assert(layer->frame.origin.y < DISP_ROWS);

  // Transition away from the watchface
  s_is_watchface_running = false;
  timeline_peek_set_visible(false, false /* animated */);
  // For simplicity, the implementation also moves the layer even though it is not necessary.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Peek service hides the peek UI using the animated code path.
  prv_set_timeline_item(NULL, true /* animated */);
  // This time we set the item to NULL, not just request invisibility. Since we're not in the
  // watchface, even though `animated` was requested, it should immediately move the position.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Transition back to the watchface
  s_is_watchface_running = true;
  timeline_peek_set_visible(true, false /* animated */);
  // Peek should be visible again, but it should still be off-screen.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);
  timeline_item_destroy(item);
}

void test_timeline_peek__peek_hidden_to_visible_outside_of_watchface(void) {
  prv_set_timeline_item(NULL, false /* animated */);
  TimelinePeek *peek = timeline_peek_get_peek();
  const Layer *layer = &peek->layout_layer;
  // Normally it is animated, but for this unit test, we don't request `animated`
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Transition away from the watchface
  s_is_watchface_running = false;
  timeline_peek_set_visible(false, false /* animated */);
  // For simplicity, the implementation also moves the layer even though it is not necessary.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Peek service shows the peek UI using the animated code path.
  TimelineItem *item = prv_set_timeline_item(&(TimelinePeekItemConfig) {
    .title = "CoreUX Design x Eng",
    .subtitle = "ConfRM-Missile Command",
    .icon = TIMELINE_RESOURCE_TIMELINE_CALENDAR,
    .num_concurrent = 0,
  }, true /* animated */);
  // Since we're not in the watchface, the peek remains off-screen.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Transition back to the watchface
  s_is_watchface_running = true;
  timeline_peek_set_visible(true, false /* animated */);
  // Peek should be visible again and now on-screen.
  cl_assert(layer->frame.origin.y < DISP_ROWS);
  timeline_item_destroy(item);
}

void test_timeline_peek__peek_visible_leaving_and_entering_watchface(void) {
  TimelineItem *item = prv_set_timeline_item(&(TimelinePeekItemConfig) {
    .title = "CoreUX Design x Eng",
    .subtitle = "ConfRM-Missile Command",
    .icon = TIMELINE_RESOURCE_TIMELINE_CALENDAR,
    .num_concurrent = 0,
  }, false /* animated */);
  TimelinePeek *peek = timeline_peek_get_peek();
  const Layer *layer = &peek->layout_layer;
  // Normally it is animated, but for this unit test, we don't request `animated`
  cl_assert(layer->frame.origin.y < DISP_ROWS);

  // Transition away from the watchface
  s_is_watchface_running = false;
  timeline_peek_set_visible(false, false /* animated */);
  // For simplicity, the implementation also moves the layer even though it is not necessary.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Transition back to the watchface
  s_is_watchface_running = true;
  timeline_peek_set_visible(true, false /* animated */);
  // Peek should be visible again and on-screen.
  cl_assert(layer->frame.origin.y < DISP_ROWS);
  timeline_item_destroy(item);
}

void test_timeline_peek__peek_hidden_leaving_and_entering_watchface(void) {
  prv_set_timeline_item(NULL, true /* animated */);
  TimelinePeek *peek = timeline_peek_get_peek();
  const Layer *layer = &peek->layout_layer;
  // Peek should be off-screen.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Transition away from the watchface
  s_is_watchface_running = false;
  timeline_peek_set_visible(false, false /* animated */);
  // Peek should be hidden and off-screen.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);

  // Transition back to the watchface
  s_is_watchface_running = true;
  timeline_peek_set_visible(true, false /* animated */);
  // Peek should be visible again, but it should still be off-screen.
  cl_assert(layer->frame.origin.y >= DISP_ROWS);
}
