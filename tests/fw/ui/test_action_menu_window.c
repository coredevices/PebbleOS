/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/action_menu_hierarchy.h"
#include "applib/ui/action_menu_layer.h"
#include "applib/ui/action_menu_window.h"
#include "applib/ui/action_menu_window_private.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/content_indicator.h"
#include "applib/ui/content_indicator_private.h"
#include "apps/system_apps/settings/settings_notifications_private.h"
#include "resource/resource.h"
#include "shell/system_theme.h"
#include "system/passert.h"
#include "util/graphics.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/size.h"

#include "clar.h"

#include <stdio.h>

static GContext s_ctx;

// Fakes
/////////////////////

#include "fake_content_indicator.h"
#include "fake_spi_flash.h"
#include "../../fixtures/load_test_resources.h"

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_bootbits.h"
#include "stubs_buffer.h"
#include "stubs_click.h"
#include "stubs_heap.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscall_internal.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

int16_t interpolate_int16(int32_t normalized, int16_t from, int16_t to) {
  return to;
}

AnimationProgress animation_timing_scaled(AnimationProgress time_normalized,
                                          AnimationProgress interval_start,
                                          AnimationProgress interval_end) {
  return interval_end;
}

int64_t interpolate_moook(int32_t normalized, int64_t from, int64_t to) {
  return to;
}

uint32_t interpolate_moook_duration() {
  return 0;
}

// Helper Functions
/////////////////////

#include "../graphics/test_graphics.h"
#include "../graphics/util.h"

// Setup and Teardown
////////////////////////////////////

static FrameBuffer *fb = NULL;
static GBitmap *s_dest_bitmap;

// To easily render multiple windows in a single canvas, we'll use an 8-bit bitmap for color
// displays (including round), but we can use the native format for black and white displays (1-bit)
#define CANVAS_GBITMAP_FORMAT PBL_IF_COLOR_ELSE(GBitmapFormat8Bit, GBITMAP_NATIVE_FORMAT)

// Overrides same function in graphics.c; we need to do this so we can pass in the GBitmapFormat
// we need to use for the unit test output canvas instead of relying on GBITMAP_NATIVE_FORMAT, which
// wouldn't work for Spalding since it uses GBitmapFormat8BitCircular
GBitmap* graphics_capture_frame_buffer(GContext *ctx) {
  PBL_ASSERTN(ctx);
  return graphics_capture_frame_buffer_format(ctx, CANVAS_GBITMAP_FORMAT);
}

// Overrides same function in graphics.c; we need to do this so we can release the framebuffer we're
// using even though its format doesn't match GBITMAP_NATIVE_FORMAT (see comment for mocked
// graphics_capture_frame_buffer() above)
bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) {
  PBL_ASSERTN(ctx);
  ctx->lock = false;
  framebuffer_dirty_all(ctx->parent_framebuffer);
  return true;
}

void test_action_menu_window__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&s_ctx, fb);
  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME, false /* is_next */);

  resource_init();
}

void test_action_menu_window__cleanup(void) {
  free(fb);

  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = NULL;
}

// Helpers
//////////////////////

static void prv_action_menu_did_close_cb(ActionMenu *action_menu,
                                         const ActionMenuItem *item,
                                         void *context) {
  ActionMenuLevel *root_level = action_menu_get_root_level(action_menu);
  action_menu_hierarchy_destroy(root_level, NULL, NULL);
}

static void prv_noop_action_callback(ActionMenu *action_menu, const ActionMenuItem *action,
                                     void *context) {
  // Do nothing
}

// From action_menu_layer.c, needed to scroll the action menu layer to the point of interest
void prv_set_selected_index(ActionMenuLayer *aml, int selected_index, bool animated);

typedef enum {
  ActionMenuLayerLongLabelScrollingAnimationState_Top,
  ActionMenuLayerLongLabelScrollingAnimationState_Middle,
  ActionMenuLayerLongLabelScrollingAnimationState_Bottom,
  ActionMenuLayerLongLabelScrollingAnimationStateCount
} ActionMenuLayerLongLabelScrollingAnimationState;

void prv_set_cell_offset(void *subject, int16_t value);

static void prv_update_cell_for_long_label_scrolling_animation_state(
    ActionMenuLayer *aml, ActionMenuLayerLongLabelScrollingAnimationState state) {
  const bool item_animation_is_valid = (aml && aml->item_animation.animation);
  if (item_animation_is_valid) {
    ActionMenuItemAnimation *item_animation = &aml->item_animation;
    int16_t new_cell_origin_y = 0;
    switch (state) {
      case ActionMenuLayerLongLabelScrollingAnimationState_Top:
        new_cell_origin_y = item_animation->bottom_offset_y;
        break;
      case ActionMenuLayerLongLabelScrollingAnimationState_Middle:
        new_cell_origin_y = (item_animation->top_offset_y + item_animation->bottom_offset_y) / 2;
        break;
      case ActionMenuLayerLongLabelScrollingAnimationState_Bottom:
        new_cell_origin_y = item_animation->top_offset_y;
        break;
      default:
        return;
    }

    prv_set_cell_offset(aml, new_cell_origin_y);
  }
}

static void prv_render_action_menu_window(const ActionMenuLevel *root_level,
                                          unsigned int selected_index,
                                          ActionMenuLayerLongLabelScrollingAnimationState state,
                                          unsigned int additional_crumbs) {
  ActionMenuConfig config = {
    .root_level = root_level,
    .colors.background = GColorChromeYellow,
    .did_close = prv_action_menu_did_close_cb,
  };

  ActionMenu *action_menu_window = app_action_menu_open(&config);

  // Set the window on screen so its window handlers will be called
  window_set_on_screen(&action_menu_window->window, true, true);

  // Scroll down to the selected index
  ActionMenuData *data = window_get_user_data(&action_menu_window->window);
  data->view_model.num_dots += additional_crumbs;
  data->crumbs_layer.level += additional_crumbs;
  prv_set_selected_index(&data->action_menu_layer, selected_index, false /* animated */);

  // Render the window so that we set the state of the cells again now that we've scrolled
  window_render(&action_menu_window->window, &s_ctx);

  // Update the animation state of the selected cell
  prv_update_cell_for_long_label_scrolling_animation_state(&data->action_menu_layer, state);

  // Render the window (for real this time)!
  window_render(&action_menu_window->window, &s_ctx);
}

#define GRID_CELL_PADDING 5

typedef void (*RenderCallback)(SettingsContentSize content_size, const ActionMenuLevel *root_level,
                               unsigned int selected_index, unsigned int additional_crumbs);

static void prv_prepare_canvas_and_render_for_each_size(RenderCallback callback,
                                                        const ActionMenuLevel *root_level,
                                                        unsigned int selected_index,
                                                        unsigned int num_rows,
                                                        unsigned int additional_crumbs) {
  gbitmap_destroy(s_dest_bitmap);

  const unsigned int num_columns = SettingsContentSizeCount;

  const int16_t bitmap_width = (DISP_COLS * num_columns) + (GRID_CELL_PADDING * (num_columns + 1));
  const int16_t bitmap_height = (num_rows == 1) ? DISP_ROWS :
      ((DISP_ROWS * num_rows) + (GRID_CELL_PADDING * (num_rows + 1)));
  const GSize bitmap_size = GSize(bitmap_width, bitmap_height);
  s_dest_bitmap = gbitmap_create_blank(bitmap_size, CANVAS_GBITMAP_FORMAT);

  s_ctx.dest_bitmap = *s_dest_bitmap;
  s_ctx.draw_state.clip_box.size = bitmap_size;
  s_ctx.draw_state.drawing_box.size = bitmap_size;

  // Fill the bitmap with pink (on color) or white (on b&w) so it's easier to see errors
  memset(s_dest_bitmap->addr, PBL_IF_COLOR_ELSE(GColorShockingPinkARGB8, GColorWhiteARGB8),
         s_dest_bitmap->row_size_bytes * s_dest_bitmap->bounds.size.h);

  for (SettingsContentSize content_size = 0; content_size < SettingsContentSizeCount;
       content_size++) {
    system_theme_set_content_size(settings_content_size_to_preferred_size(content_size));
    callback(content_size, root_level, selected_index, additional_crumbs);
  }
}

static void prv_render_action_menus_static(
    SettingsContentSize content_size, const ActionMenuLevel *root_level,
    unsigned int selected_index, unsigned int additional_crumbs) {
    const int16_t x_offset = GRID_CELL_PADDING + (content_size * (GRID_CELL_PADDING + DISP_COLS));
    s_ctx.draw_state.drawing_box.origin = GPoint(x_offset, 0);

    prv_render_action_menu_window(root_level, selected_index,
                                  ActionMenuLayerLongLabelScrollingAnimationState_Top,
                                  additional_crumbs);
}

static void prv_render_action_menus_animated(
    SettingsContentSize content_size, const ActionMenuLevel *root_level,
    unsigned int selected_index, unsigned int additional_crumbs) {
    const int16_t x_offset = GRID_CELL_PADDING + (content_size * (GRID_CELL_PADDING + DISP_COLS));

    for (ActionMenuLayerLongLabelScrollingAnimationState animation_state = 0;
         animation_state < ActionMenuLayerLongLabelScrollingAnimationStateCount;
         animation_state++) {
      const int16_t y_offset = GRID_CELL_PADDING +
                                    (animation_state * (GRID_CELL_PADDING + DISP_ROWS));
      s_ctx.draw_state.drawing_box.origin = GPoint(x_offset, y_offset);
      prv_render_action_menu_window(root_level, selected_index, animation_state,
                                    additional_crumbs);
    }
}

void prv_prepare_canvas_and_render_action_menus_static(const ActionMenuLevel *root_level,
                                                       unsigned int selected_index,
                                                       unsigned int additional_crumbs) {
  prv_prepare_canvas_and_render_for_each_size(
      prv_render_action_menus_static, root_level, selected_index, 1 /* num_rows */,
      additional_crumbs);
}

void prv_prepare_canvas_and_render_action_menus_animated(const ActionMenuLevel *root_level,
                                                         unsigned int selected_index) {
  prv_prepare_canvas_and_render_for_each_size(
      prv_render_action_menus_animated, root_level, selected_index,
      ActionMenuLayerLongLabelScrollingAnimationStateCount,
      0 /* additional_crumbs */);
}

// Tests
//////////////////////

void test_action_menu_window__wide_display_mode_with_just_titles(void) {
  ActionMenuLevel *root_level = action_menu_level_create(3);
  action_menu_level_add_action(root_level,
                               "I will text back",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_action(root_level,
                               "Sorry, I can't talk right now, call me back at a later time",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_action(root_level,
                               "I will call back",
                               prv_noop_action_callback,
                               NULL);

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_animated(root_level, selected_index);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__thin_display_mode_with_emoji(void) {
  // Copied from prv_create_emoji_level_from_action() in timeline_actions.c; it wouldn't work that
  // well to just make the array T_STATIC in that function because we need to know its length too
  static const char* thin_values[] = {
          "😃",
          "😉",
          "😂",
          "😍",
          "😘",
          "\xe2\x9d\xa4",
          "😇",
          "😎",
          "😛",
          "😟",
          "😩",
          "😭",
          "😴",
          "😐",
          "😯",
          "👍",
          "👎",
          "👌",
          "💩",
          "🎉",
          "🍺",
  };
  ActionMenuLevel *root_level = action_menu_level_create(ARRAY_LENGTH(thin_values));
  action_menu_level_set_display_mode(root_level, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < ARRAY_LENGTH(thin_values); i++) {
    action_menu_level_add_action(root_level, thin_values[i], prv_noop_action_callback, NULL);
  }

  const unsigned int selected_index = 0;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__thin_display_mode_two_row(void) {
  static const char* thin_values[] = { "a", "b", "c", "d", "e" };
  ActionMenuLevel *root_level = action_menu_level_create(ARRAY_LENGTH(thin_values));
  action_menu_level_set_display_mode(root_level, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < ARRAY_LENGTH(thin_values); i++) {
    action_menu_level_add_action(root_level, thin_values[i], prv_noop_action_callback, NULL);
  }

  const unsigned int selected_index = 4;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__thin_display_mode_one_row(void) {
  static const char* thin_values[] = { "Y", "N" };
  ActionMenuLevel *root_level = action_menu_level_create(ARRAY_LENGTH(thin_values));
  action_menu_level_set_display_mode(root_level, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < ARRAY_LENGTH(thin_values); i++) {
    action_menu_level_add_action(root_level, thin_values[i], prv_noop_action_callback, NULL);
  }

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__thin_display_mode_one_item(void) {
  static const char* thin_values[] = { "Y" };
  ActionMenuLevel *root_level = action_menu_level_create(ARRAY_LENGTH(thin_values));
  action_menu_level_set_display_mode(root_level, ActionMenuLevelDisplayModeThin);
  for (size_t i = 0; i < ARRAY_LENGTH(thin_values); i++) {
    action_menu_level_add_action(root_level, thin_values[i], prv_noop_action_callback, NULL);
  }

  const unsigned int selected_index = 0;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_chevron(void) {
  ActionMenuLevel *root_level = action_menu_level_create(3);
  ActionMenuLevel *voice_level = action_menu_level_create(1);
  action_menu_level_add_action(voice_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, voice_level, "Voice");

  ActionMenuLevel *template_level = action_menu_level_create(1);
  action_menu_level_add_action(template_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, template_level, "Template");

  ActionMenuLevel *emoji_level = action_menu_level_create(1);
  action_menu_level_add_action(emoji_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, emoji_level, "Emoji");

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_chevron_and_long_labels(void) {
  ActionMenuLevel *root_level = action_menu_level_create(3);
  ActionMenuLevel *voice_level = action_menu_level_create(1);
  action_menu_level_add_action(voice_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, voice_level, "I will text back");

  ActionMenuLevel *template_level = action_menu_level_create(1);
  action_menu_level_add_action(template_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, template_level,
                              "Sorry, I can't talk right now, call me back at a later time");

  ActionMenuLevel *emoji_level = action_menu_level_create(1);
  action_menu_level_add_action(emoji_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, emoji_level, "I will call back");

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_animated(root_level, selected_index);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_chevron_and_long_labels_hyphenated(void) {
  ActionMenuLevel *root_level = action_menu_level_create(3);
  ActionMenuLevel *voice_level = action_menu_level_create(1);
  action_menu_level_add_action(voice_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, voice_level, "Dismiss");

  ActionMenuLevel *template_level = action_menu_level_create(1);
  action_menu_level_add_action(template_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, template_level,
                              "Reply to HUBERT BLAINE WOLFESCHLEGELSTEINHAUSENBERGERDORFF");

  ActionMenuLevel *emoji_level = action_menu_level_create(1);
  action_menu_level_add_action(emoji_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, emoji_level, "Open on phone");

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_animated(root_level, selected_index);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_separator(void) {
  ActionMenuLevel *root_level = action_menu_level_create(3);
  action_menu_level_add_action(root_level,
                               "Change Time",
                               prv_noop_action_callback,
                               NULL);

  action_menu_level_add_action(root_level,
                               "Change Days",
                               prv_noop_action_callback,
                               NULL);

  ActionMenuLevel *snooze_level = action_menu_level_create(1);
  action_menu_level_add_action(snooze_level,
                               "This won't be seen",
                               prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, snooze_level, "Snooze Delay");

  root_level->separator_index = root_level->num_items - 1;

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 1);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}
