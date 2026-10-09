/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/util/hash.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/testing.h>

#include <applib/fonts/fonts.h>
#include <applib/graphics/framebuffer.h>
#include <applib/graphics/graphics.h>
#include <applib/graphics/raw_image.h>
#include <applib/ui/action_menu_hierarchy.h>
#include <applib/ui/action_menu_layer.h>
#include <applib/ui/action_menu_window.h>
#include <applib/ui/action_menu_window_private.h>
#include <applib/ui/app_window_stack.h>
#include <applib/ui/content_indicator.h>
#include <applib/ui/content_indicator_private.h>
#include <apps/system/settings/notifications_private.h>
#include <clar.h>
#include <resource/resource.h>
#include <shell/system_theme.h>
#include <system/passert.h>

static GContext s_ctx;

// Fakes
/////////////////////

#include "../../fixtures/load_test_resources.h"

#include <fake_content_indicator.h>
#include <fake_spi_flash.h>

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Stubs
/////////////////////

#include <stubs_analytics.h>
#include <stubs_app_install_manager.h>
#include <stubs_app_state.h>
#include <stubs_app_timer.h>
#include <stubs_bootbits.h>
#include <stubs_buffer.h>
#include <stubs_click.h>
#include <stubs_heap.h>
#include <stubs_layer.h>
#include <stubs_logging.h>
#include <stubs_memory_layout.h>
#include <stubs_mutex.h>
#include <stubs_passert.h>
#include <stubs_pbl_malloc.h>
#include <stubs_pebble_tasks.h>
#include <stubs_print.h>
#include <stubs_process_manager.h>
#include <stubs_serial.h>
#include <stubs_shell_prefs.h>
#include <stubs_sleep.h>
#include <stubs_status_bar_layer.h>
#include <stubs_syscall_internal.h>
#include <stubs_syscalls.h>
#include <stubs_task_wdt.h>
#include <stubs_vibes.h>
#include <stubs_window_manager.h>
#include <stubs_window_stack.h>

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
GBitmap *graphics_capture_frame_buffer(GContext *ctx) {
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
  framebuffer_init(fb, &(GSize){DISP_COLS, DISP_ROWS});
  test_graphics_context_init(&s_ctx, fb);
  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);

  resource_init();
}

void test_action_menu_window__cleanup(void) {
  free(fb);

  gbitmap_destroy(s_dest_bitmap);
  s_dest_bitmap = NULL;
}

// Helpers
//////////////////////

static void prv_action_menu_did_close_cb(ActionMenu *action_menu, const ActionMenuItem *item,
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
    .colors.background = (root_level->display_mode == ActionMenuLevelDisplayModeGlyphGrid)
                             ? GColorFolly
                             : GColorChromeYellow,
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
  const int16_t bitmap_height =
      (num_rows == 1) ? DISP_ROWS : ((DISP_ROWS * num_rows) + (GRID_CELL_PADDING * (num_rows + 1)));
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

static void prv_render_action_menus_static(SettingsContentSize content_size,
                                           const ActionMenuLevel *root_level,
                                           unsigned int selected_index,
                                           unsigned int additional_crumbs) {
  const int16_t x_offset = GRID_CELL_PADDING + (content_size * (GRID_CELL_PADDING + DISP_COLS));
  s_ctx.draw_state.drawing_box.origin = GPoint(x_offset, 0);

  prv_render_action_menu_window(root_level, selected_index,
                                ActionMenuLayerLongLabelScrollingAnimationState_Top,
                                additional_crumbs);
}

static void prv_render_action_menus_animated(SettingsContentSize content_size,
                                             const ActionMenuLevel *root_level,
                                             unsigned int selected_index,
                                             unsigned int additional_crumbs) {
  const int16_t x_offset = GRID_CELL_PADDING + (content_size * (GRID_CELL_PADDING + DISP_COLS));

  for (ActionMenuLayerLongLabelScrollingAnimationState animation_state = 0;
       animation_state < ActionMenuLayerLongLabelScrollingAnimationStateCount; animation_state++) {
    const int16_t y_offset =
        GRID_CELL_PADDING + (animation_state * (GRID_CELL_PADDING + DISP_ROWS));
    s_ctx.draw_state.drawing_box.origin = GPoint(x_offset, y_offset);
    prv_render_action_menu_window(root_level, selected_index, animation_state, additional_crumbs);
  }
}

void prv_prepare_canvas_and_render_action_menus_static(const ActionMenuLevel *root_level,
                                                       unsigned int selected_index,
                                                       unsigned int additional_crumbs) {
  prv_prepare_canvas_and_render_for_each_size(prv_render_action_menus_static, root_level,
                                              selected_index, 1 /* num_rows */, additional_crumbs);
}

void prv_prepare_canvas_and_render_action_menus_animated(const ActionMenuLevel *root_level,
                                                         unsigned int selected_index) {
  prv_prepare_canvas_and_render_for_each_size(
      prv_render_action_menus_animated, root_level, selected_index,
      ActionMenuLayerLongLabelScrollingAnimationStateCount, 0 /* additional_crumbs */);
}

// Tests
//////////////////////

void test_action_menu_window__wide_display_mode_with_just_titles(void) {
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  ActionMenuLevel *root_level = action_menu_level_create(3);
  action_menu_level_add_action(root_level, "I will text back", prv_noop_action_callback, NULL);
  action_menu_level_add_action(root_level,
                               "Sorry, I can't talk right now, call me back at a later time",
                               prv_noop_action_callback, NULL);
  action_menu_level_add_action(root_level, "I will call back", prv_noop_action_callback, NULL);

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_animated(root_level, selected_index);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

static void prv_render_emoji_picker(unsigned int selected_index, bool glyph_grid) {
  // Copied from prv_create_emoji_level_from_action() in timeline_actions.c; it wouldn't work that
  // well to just make the array PBL_T_STATIC in that function because we need to know its length
  // too
  static const char *thin_values[] = {
    "😃", "😉", "😂", "😍", "😘", "\xe2\x9d\xa4", "😇", "😎", "😛", "😟", "😩",
    "😭", "😴", "😐", "😯", "👍", "👎",           "👌", "💩", "🎉", "🍺",
  };
  ActionMenuLevel *root_level = action_menu_level_create(ARRAY_LENGTH(thin_values));
  root_level->display_mode =
      glyph_grid ? ActionMenuLevelDisplayModeGlyphGrid : ActionMenuLevelDisplayModeThin;
  for (size_t i = 0; i < ARRAY_LENGTH(thin_values); i++) {
    action_menu_level_add_action(root_level, thin_values[i], prv_noop_action_callback, NULL);
  }

  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
}

static void prv_check_emoji_picker(unsigned int selected_index, const char *filename) {
#if !PBL_COLOR
  // The glyph grid must fall back to the legacy thin rendering on monochrome displays.
  prv_render_emoji_picker(selected_index, false);
  GBitmap *legacy = s_dest_bitmap;
  s_dest_bitmap = NULL;
  prv_render_emoji_picker(selected_index, true);
  cl_check(gbitmap_eq(s_dest_bitmap, legacy, filename));
  gbitmap_destroy(legacy);
#else
  prv_render_emoji_picker(selected_index, true);
#endif
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, filename));
}

void test_action_menu_window__thin_display_mode_with_emoji(void) {
  prv_check_emoji_picker(0, TEST_PBI_FILE);
  const GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_28_EMOJI);
  cl_assert_equal_i(HAS_FEATURE(font->base.md.version, VERSION_FIELD_FEATURE_COLOR) != 0,
                    PBL_IF_COLOR_ELSE(true, false));
}

void test_action_menu_window__emoji_picker_snooze(void) {
  prv_check_emoji_picker(12, TEST_PBI_FILE);
}

void test_action_menu_window__emoji_picker_last_page(void) {
  prv_check_emoji_picker(20, TEST_PBI_FILE);
}

void test_action_menu_window__glyph_grid_not_settable_from_sdk(void) {
  ActionMenuLevel *level = action_menu_level_create(1);
  action_menu_level_set_display_mode(level, ActionMenuLevelDisplayModeGlyphGrid);
  cl_assert_equal_i(level->display_mode, ActionMenuLevelDisplayModeWide);
  action_menu_hierarchy_destroy(level, NULL, NULL);
}

void test_action_menu_window__wide_items_reset_glyph_grid(void) {
  ActionMenuLevel *root_level = action_menu_level_create(3);
  root_level->display_mode = ActionMenuLevelDisplayModeGlyphGrid;
  for (int i = 0; i < 3; ++i) {
    action_menu_level_add_action(root_level, "😃", prv_noop_action_callback, NULL);
  }
  ActionMenuConfig config = {
    .root_level = root_level,
    .did_close = prv_action_menu_did_close_cb,
  };
  ActionMenu *action_menu = app_action_menu_open(&config);
  window_set_on_screen(&action_menu->window, true, true);
  ActionMenuData *data = window_get_user_data(&action_menu->window);
  ActionMenuLayer *aml = &data->action_menu_layer;
  cl_assert(aml->glyph_grid);

  action_menu_layer_set_short_items(aml, NULL, 0, 0);
  action_menu_layer_set_items(aml, root_level->items, 3, 0, 0);
  cl_assert(!aml->glyph_grid);
  cl_assert(aml->layout_cache.font != fonts_get_system_font(FONT_KEY_GOTHIC_28_EMOJI));
}

void test_action_menu_window__thin_display_mode_two_row(void) {
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  static const char *thin_values[] = {"a", "b", "c", "d", "e"};
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
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  static const char *thin_values[] = {"Y", "N"};
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
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  static const char *thin_values[] = {"Y"};
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
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  ActionMenuLevel *root_level = action_menu_level_create(3);
  ActionMenuLevel *voice_level = action_menu_level_create(1);
  action_menu_level_add_action(voice_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, voice_level, "Voice");

  ActionMenuLevel *template_level = action_menu_level_create(1);
  action_menu_level_add_action(template_level, "This won't be seen", prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, template_level, "Template");

  ActionMenuLevel *emoji_level = action_menu_level_create(1);
  action_menu_level_add_action(emoji_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, emoji_level, "Emoji");

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 0);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_chevron_and_long_labels(void) {
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  ActionMenuLevel *root_level = action_menu_level_create(3);
  ActionMenuLevel *voice_level = action_menu_level_create(1);
  action_menu_level_add_action(voice_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, voice_level, "I will text back");

  ActionMenuLevel *template_level = action_menu_level_create(1);
  action_menu_level_add_action(template_level, "This won't be seen", prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, template_level,
                              "Sorry, I can't talk right now, call me back at a later time");

  ActionMenuLevel *emoji_level = action_menu_level_create(1);
  action_menu_level_add_action(emoji_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, emoji_level, "I will call back");

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_animated(root_level, selected_index);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_chevron_and_long_labels_hyphenated(void) {
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  ActionMenuLevel *root_level = action_menu_level_create(3);
  ActionMenuLevel *voice_level = action_menu_level_create(1);
  action_menu_level_add_action(voice_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, voice_level, "Dismiss");

  ActionMenuLevel *template_level = action_menu_level_create(1);
  action_menu_level_add_action(template_level, "This won't be seen", prv_noop_action_callback,
                               NULL);
  action_menu_level_add_child(root_level, template_level,
                              "Reply to HUBERT BLAINE WOLFESCHLEGELSTEINHAUSENBERGERDORFF");

  ActionMenuLevel *emoji_level = action_menu_level_create(1);
  action_menu_level_add_action(emoji_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, emoji_level, "Open on phone");

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_animated(root_level, selected_index);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

void test_action_menu_window__wide_display_mode_with_separator(void) {
  if (!PBL_COLOR) {
    return; // no monochrome baseline
  }
  ActionMenuLevel *root_level = action_menu_level_create(3);
  action_menu_level_add_action(root_level, "Change Time", prv_noop_action_callback, NULL);

  action_menu_level_add_action(root_level, "Change Days", prv_noop_action_callback, NULL);

  ActionMenuLevel *snooze_level = action_menu_level_create(1);
  action_menu_level_add_action(snooze_level, "This won't be seen", prv_noop_action_callback, NULL);
  action_menu_level_add_child(root_level, snooze_level, "Snooze Delay");

  root_level->separator_index = root_level->num_items - 1;

  const unsigned int selected_index = 1;
  prv_prepare_canvas_and_render_action_menus_static(root_level, selected_index, 1);
  cl_check(gbitmap_pbi_eq(s_dest_bitmap, TEST_PBI_FILE));
}

static int s_freezing_action_count;
static const ActionMenuItem *s_last_emoji_action;

static void prv_emoji_action_callback(ActionMenu *action_menu, const ActionMenuItem *action,
                                      void *context) {
  s_last_emoji_action = action;
  action_menu_freeze(action_menu);
}

void test_action_menu_window__emoji_picker_selection(void) {
  ActionMenuLevel *root_level = action_menu_level_create(21);
  root_level->display_mode = ActionMenuLevelDisplayModeGlyphGrid;
  for (int i = 0; i < 21; ++i) {
    action_menu_level_add_action(root_level, "😃", prv_emoji_action_callback, NULL);
  }
  ActionMenuConfig config = {
    .root_level = root_level,
    .did_close = prv_action_menu_did_close_cb,
  };
  ActionMenu *action_menu = app_action_menu_open(&config);
  window_set_on_screen(&action_menu->window, true, true);
  ActionMenuData *data = window_get_user_data(&action_menu->window);
  ActionMenuLayer *aml = &data->action_menu_layer;
  MenuLayer *menu = &aml->menu_layer;
#if PBL_COLOR
  cl_assert_equal_i(fonts_get_font_height(aml->layout_cache.font), 28);
#else
  const GFont font = aml->layout_cache.font;
  const PreferredContentSize size = system_theme_get_content_size();
  system_theme_set_content_size(PreferredContentSizeSmall);
  action_menu_layer_set_short_items(aml, root_level->items, 21, 0);
  cl_assert(aml->layout_cache.font == font);
  system_theme_set_content_size(size);
#endif

  [[maybe_unused]] static const int round_rows[] = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
                                                    1, 1, 1, 2, 2, 2, 2, 2, 2, 2};
  for (int count = 0; count <= 21; ++count) {
    action_menu_layer_set_short_items(aml, root_level->items, count, 0);
    const int expected_rows = PBL_IF_ROUND_ELSE((count + 9) / 10, (count + 2) / 3);
    cl_assert_equal_i(menu->callbacks.get_num_rows(menu, 0, aml), expected_rows);
#if PBL_ROUND
    int page_counts[3] = {};
    for (int i = 0; i < count; ++i) {
      prv_set_selected_index(aml, i, false);
      const int page = menu_layer_get_selected_index(menu).row;
      cl_assert(page >= 0 && page < expected_rows);
      ++page_counts[page];
    }
    for (int page = 0; page < expected_rows; ++page) {
      cl_assert(page_counts[page] > 0 && page_counts[page] <= 10);
      cl_assert(page_counts[page] >= page_counts[0] - 1);
      cl_assert(page_counts[page] <= page_counts[0] + 1);
    }
#endif
  }

  for (int i = 0; i < 21; ++i) {
    prv_set_selected_index(aml, i, false);
    MenuIndex selected = menu_layer_get_selected_index(menu);
    cl_assert_equal_i(selected.row, PBL_IF_ROUND_ELSE(round_rows[i], i / 3));
    s_last_emoji_action = NULL;
    menu->callbacks.select_click(menu, &selected, menu->callback_context);
    cl_assert(s_last_emoji_action == &root_level->items[i]);
    action_menu_unfreeze(action_menu);
  }
  prv_set_selected_index(aml, 21, false);
  cl_assert_equal_i(aml->selected_index, 20);
  prv_set_selected_index(aml, -1, false);
  cl_assert_equal_i(aml->selected_index, 0);
}

static void prv_freezing_action_callback(ActionMenu *action_menu, const ActionMenuItem *action,
                                         void *context) {
  s_freezing_action_count++;
  action_menu_freeze(action_menu);
}

void test_action_menu_window__frozen_ignores_tap(void) {
  ActionMenuLevel *root_level = action_menu_level_create(2);
  action_menu_level_add_action(root_level, "Send to phone", prv_freezing_action_callback, NULL);
  action_menu_level_add_action(root_level, "Other", prv_noop_action_callback, NULL);

  ActionMenuConfig config = {
    .root_level = root_level,
    .did_close = prv_action_menu_did_close_cb,
  };
  ActionMenu *action_menu = app_action_menu_open(&config);
  window_set_on_screen(&action_menu->window, true, true);

  ActionMenuData *data = window_get_user_data(&action_menu->window);
  MenuLayer *menu_layer = &data->action_menu_layer.menu_layer;
  MenuIndex index = MenuIndex(0, 0);

  s_freezing_action_count = 0;
  menu_layer->callbacks.select_click(menu_layer, &index, menu_layer->callback_context);
  cl_assert(action_menu_is_frozen(action_menu));
  menu_layer->callbacks.select_click(menu_layer, &index, menu_layer->callback_context);
  cl_assert_equal_i(s_freezing_action_count, 1);

  action_menu_unfreeze(action_menu);
  menu_layer->callbacks.select_click(menu_layer, &index, menu_layer->callback_context);
  cl_assert_equal_i(s_freezing_action_count, 2);
}
